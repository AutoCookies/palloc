/* ----------------------------------------------------------------------------
Copyright (c) 2018-2026, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#include "palloc.h"
#include "palloc/internal.h"
#include "palloc/atomic.h"
#include "palloc/prim.h"
#include "palloc/arena_pomai.h"

/*
 * Vector arena: single contiguous block via _pa_prim_alloc.
 * Header at start of block; payload follows at SIMD-aligned offset.
 * Zero-OOM: strict capacity_bytes limit; alloc returns NULL when full.
 */

typedef struct pa_arena_vector_s {
  _Atomic(uintptr_t) offset_atomic;  /* used only when shared == true */
  uintptr_t          offset_plain;    /* used only when shared == false */
  size_t             payload_start;   /* first byte available for allocations (aligned) */
  size_t             payload_capacity;
  size_t             total_size;
  bool               shared;
  uint8_t            _pad[7];
} pa_arena_vector_t;

#define PA_ARENA_VECTOR_HEADER_SIZE  (sizeof(pa_arena_vector_t))

/* pa_arena_t is the public opaque type; it points to this struct. */
typedef pa_arena_vector_t pa_arena_impl_t;

static inline size_t pa_arena_payload_start(void) {
  return (size_t)_pa_align_up((uintptr_t)PA_ARENA_VECTOR_HEADER_SIZE, (uintptr_t)PA_ARENA_VECTOR_ALIGN);
}

pa_arena_t* p_arena_create_for_vector(size_t capacity_bytes) pa_attr_noexcept {
  return p_arena_create_for_vector_ex(capacity_bytes, false);
}

pa_arena_t* p_arena_create_for_vector_ex(size_t capacity_bytes, bool shared) pa_attr_noexcept {
  if (capacity_bytes == 0) return NULL;

  size_t payload_start = pa_arena_payload_start();
  size_t total_size = _pa_align_up(payload_start + capacity_bytes, _pa_os_page_size());

  bool is_large = false;
  bool is_zero = false;
  void* p = NULL;
  int err = _pa_prim_alloc(NULL, total_size, _pa_os_page_size(), true, true, &is_large, &is_zero, &p);
  if (err != 0 || p == NULL) return NULL;

  pa_arena_vector_t* arena = (pa_arena_vector_t*)p;
  arena->payload_start = payload_start;
  arena->payload_capacity = capacity_bytes;
  arena->total_size = total_size;
  arena->shared = shared;

  if (shared) {
    pa_atomic_store_release(&arena->offset_atomic, (uintptr_t)payload_start);
    arena->offset_plain = 0;
  } else {
    arena->offset_plain = payload_start;
    pa_atomic_store_release(&arena->offset_atomic, (uintptr_t)payload_start); /* unused but keep in sync for reset */
  }

  return (pa_arena_t*)arena;
}

static inline uintptr_t pa_arena_capacity_limit(pa_arena_vector_t* arena) {
  return (uintptr_t)arena->payload_start + (uintptr_t)arena->payload_capacity;
}

void* p_arena_alloc_vector(pa_arena_t* arena_ptr, size_t vector_dim, size_t element_size) pa_attr_noexcept {
  if (arena_ptr == NULL) return NULL;
  if (element_size == 0 || (vector_dim > 0 && element_size > SIZE_MAX / vector_dim)) return NULL;

  size_t size = vector_dim * element_size;
  if (size == 0) return NULL;

  pa_arena_vector_t* arena = (pa_arena_vector_t*)arena_ptr;
  size_t aligned_size = (size_t)_pa_align_up((uintptr_t)size, (uintptr_t)PA_ARENA_VECTOR_ALIGN);
  uintptr_t limit = pa_arena_capacity_limit(arena);
  uintptr_t base = (uintptr_t)arena;

  if (arena->shared) {
    uintptr_t old = pa_atomic_add_acq_rel(&arena->offset_atomic, (uintptr_t)aligned_size);
    if (old + aligned_size > limit) {
      pa_atomic_add_acq_rel(&arena->offset_atomic, (uintptr_t)(-(intptr_t)aligned_size)); /* rollback */
      return NULL;
    }
    return (void*)(base + old);
  } else {
    uintptr_t cur = (uintptr_t)arena->offset_plain;
    if (cur + aligned_size > limit) return NULL;
    arena->offset_plain = cur + aligned_size;
    return (void*)(base + cur);
  }
}

void p_arena_reset(pa_arena_t* arena_ptr) pa_attr_noexcept {
  if (arena_ptr == NULL) return;
  pa_arena_vector_t* arena = (pa_arena_vector_t*)arena_ptr;
  uintptr_t start = (uintptr_t)arena->payload_start;

  if (arena->shared) {
    pa_atomic_store_release(&arena->offset_atomic, start);
  } else {
    arena->offset_plain = start;
  }
}

void p_arena_destroy(pa_arena_t* arena_ptr) pa_attr_noexcept {
  if (arena_ptr == NULL) return;
  pa_arena_vector_t* arena = (pa_arena_vector_t*)arena_ptr;
  _pa_prim_free((void*)arena, arena->total_size);
}

/* --- Legacy API (byte-based); same arena layout, capacity = size --- */

pa_decl_export void* p_arena_create(size_t size) pa_attr_noexcept {
  return (void*)p_arena_create_for_vector(size);
}

pa_decl_export void* p_arena_alloc(void* arena_ptr, size_t size) pa_attr_noexcept {
  if (arena_ptr == NULL || size == 0) return NULL;
  /* Byte-based: vector_dim = size, element_size = 1 */
  return p_arena_alloc_vector((pa_arena_t*)arena_ptr, size, 1);
}
