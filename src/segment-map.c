/* ----------------------------------------------------------------------------
Copyright (c) 2019-2023, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

/* -----------------------------------------------------------
  The following functions are to reliably find the segment or
  block that encompasses any pointer p (or NULL if it is not
  in any of our segments).
  We maintain a bitmap of all memory with 1 bit per PA_SEGMENT_SIZE (64MiB)
  set to 1 if it contains the segment meta data.
----------------------------------------------------------- */
#include "palloc.h"
#include "palloc/internal.h"
#include "palloc/atomic.h"

// Reduce total address space to reduce .bss  (due to the `pa_segment_map`)
#if (PA_INTPTR_SIZE > 4) && PA_TRACK_ASAN
#define PA_SEGMENT_MAP_MAX_ADDRESS    (128*1024ULL*PA_GiB)  // 128 TiB  (see issue #881)
#elif (PA_INTPTR_SIZE > 4)
#define PA_SEGMENT_MAP_MAX_ADDRESS    (48*1024ULL*PA_GiB)   // 48 TiB
#else
#define PA_SEGMENT_MAP_MAX_ADDRESS    (UINT32_MAX)
#endif

#define PA_SEGMENT_MAP_PART_SIZE      (PA_INTPTR_SIZE*PA_KiB - 128)      // 128 > sizeof(pa_memid_t) ! 
#define PA_SEGMENT_MAP_PART_BITS      (8*PA_SEGMENT_MAP_PART_SIZE)
#define PA_SEGMENT_MAP_PART_ENTRIES   (PA_SEGMENT_MAP_PART_SIZE / PA_INTPTR_SIZE)
#define PA_SEGMENT_MAP_PART_BIT_SPAN  (PA_SEGMENT_ALIGN)                 // memory area covered by 1 bit

#if (PA_SEGMENT_MAP_PART_BITS < (PA_SEGMENT_MAP_MAX_ADDRESS / PA_SEGMENT_MAP_PART_BIT_SPAN)) // prevent overflow on 32-bit (issue #1017)
#define PA_SEGMENT_MAP_PART_SPAN      (PA_SEGMENT_MAP_PART_BITS * PA_SEGMENT_MAP_PART_BIT_SPAN)
#else
#define PA_SEGMENT_MAP_PART_SPAN      PA_SEGMENT_MAP_MAX_ADDRESS
#endif

#define PA_SEGMENT_MAP_MAX_PARTS      ((PA_SEGMENT_MAP_MAX_ADDRESS / PA_SEGMENT_MAP_PART_SPAN) + 1)

// A part of the segment map.
typedef struct pa_segmap_part_s {
  pa_memid_t memid;
  _Atomic(uintptr_t) map[PA_SEGMENT_MAP_PART_ENTRIES];
} pa_segmap_part_t;

// Allocate parts on-demand to reduce .bss footprint
static _Atomic(pa_segmap_part_t*) pa_segment_map[PA_SEGMENT_MAP_MAX_PARTS]; // = { NULL, .. }

static pa_segmap_part_t* pa_segment_map_index_of(const pa_segment_t* segment, bool create_on_demand, size_t* idx, size_t* bitidx) {
  // note: segment can be invalid or NULL.
  pa_assert_internal(_pa_ptr_segment(segment + 1) == segment); // is it aligned on PA_SEGMENT_SIZE?
  *idx = 0;
  *bitidx = 0;  
  if ((uintptr_t)segment >= PA_SEGMENT_MAP_MAX_ADDRESS) return NULL;
  const uintptr_t segindex = ((uintptr_t)segment) / PA_SEGMENT_MAP_PART_SPAN;
  if (segindex >= PA_SEGMENT_MAP_MAX_PARTS) return NULL;
  pa_segmap_part_t* part = pa_atomic_load_ptr_relaxed(pa_segmap_part_t, &pa_segment_map[segindex]);

  // allocate on demand to reduce .bss footprint
  if pa_unlikely(part == NULL) {
    if (!create_on_demand) return NULL;
    pa_memid_t memid;
    part = (pa_segmap_part_t*)_pa_os_zalloc(sizeof(pa_segmap_part_t), &memid);
    if (part == NULL) return NULL;
    part->memid = memid;
    pa_segmap_part_t* expected = NULL;
    if (!pa_atomic_cas_ptr_strong_release(pa_segmap_part_t, &pa_segment_map[segindex], &expected, part)) {
      _pa_os_free(part, sizeof(pa_segmap_part_t), memid);
      part = expected;
      if (part == NULL) return NULL;
    }
  }
  pa_assert(part != NULL);
  const uintptr_t offset = ((uintptr_t)segment) % PA_SEGMENT_MAP_PART_SPAN;
  const uintptr_t bitofs = offset / PA_SEGMENT_MAP_PART_BIT_SPAN;
  *idx = bitofs / PA_INTPTR_BITS;
  *bitidx = bitofs % PA_INTPTR_BITS;
  return part;
}

void _pa_segment_map_allocated_at(const pa_segment_t* segment) {
  if (segment->memid.memkind == PA_MEM_ARENA) return; // we lookup segments first in the arena's and don't need the segment map
  size_t index;
  size_t bitidx;
  pa_segmap_part_t* part = pa_segment_map_index_of(segment, true /* alloc map if needed */, &index, &bitidx);
  if (part == NULL) return; // outside our address range..
  uintptr_t mask = pa_atomic_load_relaxed(&part->map[index]);
  uintptr_t newmask;
  do {
    newmask = (mask | ((uintptr_t)1 << bitidx));
  } while (!pa_atomic_cas_weak_release(&part->map[index], &mask, newmask));
}

void _pa_segment_map_freed_at(const pa_segment_t* segment) {
  if (segment->memid.memkind == PA_MEM_ARENA) return;
  size_t index;
  size_t bitidx;
  pa_segmap_part_t* part = pa_segment_map_index_of(segment, false /* don't alloc if not present */, &index, &bitidx);
  if (part == NULL) return; // outside our address range..
  uintptr_t mask = pa_atomic_load_relaxed(&part->map[index]);
  uintptr_t newmask;
  do {
    newmask = (mask & ~((uintptr_t)1 << bitidx));
  } while (!pa_atomic_cas_weak_release(&part->map[index], &mask, newmask));
}

// Determine the segment belonging to a pointer or NULL if it is not in a valid segment.
static pa_segment_t* _pa_segment_of(const void* p) {
  if (p == NULL) return NULL;
  pa_segment_t* segment = _pa_ptr_segment(p);  // segment can be NULL  
  size_t index;
  size_t bitidx;
  pa_segmap_part_t* part = pa_segment_map_index_of(segment, false /* dont alloc if not present */, &index, &bitidx);
  if (part == NULL) return NULL;  
  const uintptr_t mask = pa_atomic_load_relaxed(&part->map[index]);
  if pa_likely((mask & ((uintptr_t)1 << bitidx)) != 0) {
    bool cookie_ok = (_pa_ptr_cookie(segment) == segment->cookie);
    pa_assert_internal(cookie_ok); PA_UNUSED(cookie_ok);
    return segment; // yes, allocated by us
  }
  return NULL;
}

// Is this a valid pointer in our heap?
static bool pa_is_valid_pointer(const void* p) {
  // first check if it is in an arena, then check if it is OS allocated
  return (_pa_arena_contains(p) || _pa_segment_of(p) != NULL);
}

pa_decl_nodiscard pa_decl_export bool pa_is_in_heap_region(const void* p) pa_attr_noexcept {
  return pa_is_valid_pointer(p);
}

void _pa_segment_map_unsafe_destroy(void) {
  for (size_t i = 0; i < PA_SEGMENT_MAP_MAX_PARTS; i++) {
    pa_segmap_part_t* part = pa_atomic_exchange_ptr_relaxed(pa_segmap_part_t, &pa_segment_map[i], NULL);
    if (part != NULL) {
      _pa_os_free(part, sizeof(pa_segmap_part_t), part->memid);
    }
  }
}
