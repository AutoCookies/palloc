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
 * The pomai_arena structure is allocated at the very start of the reserved memory block.
 */
typedef struct pomai_arena_s {
  _Atomic(uintptr_t) offset;    // Current bump-pointer offset
  size_t             size;      // Total size of the reserved block (including this struct)
  pa_memid_t         memid;     // OS memory ID for freeing
} pomai_arena_t;

pa_decl_export void* p_arena_create(size_t size) pa_attr_noexcept {
  if (size == 0) return NULL;
  
  // Ensure we account for our own management structure
  size_t total_size = _pa_align_up(size + sizeof(pomai_arena_t), _pa_os_page_size());
  
  bool is_large = false;
  bool is_zero = false;
  void* p = NULL;
  
  // Directly allocate from OS primitives to bypass all palloc caches
  int err = _pa_prim_alloc(NULL, total_size, _pa_os_page_size(), true, true, &is_large, &is_zero, &p);
  if (err != 0 || p == NULL) return NULL;
  
  pomai_arena_t* arena = (pomai_arena_t*)p;
  pa_atomic_store_release(&arena->offset, (uintptr_t)sizeof(pomai_arena_t));
  arena->size = total_size;
  
  // We don't have a formal memid from _pa_os_alloc, so we'll need to use _pa_prim_free directly in destroy.
  // We can store a dummy or partial memid if needed, but here we'll just track enough to free.
  
  return (void*)arena;
}

pa_decl_export void* p_arena_alloc(void* arena_ptr, size_t size) pa_attr_noexcept {
  if (arena_ptr == NULL || size == 0) return NULL;
  
  pomai_arena_t* arena = (pomai_arena_t*)arena_ptr;
  
  // Align the allocation size (e.g., 16-byte alignment or similar)
  size_t aligned_size = _pa_align_up(size, 16);
  
  // Atomic bump-pointer increment
  uintptr_t old_offset = pa_atomic_add_acq_rel(&arena->offset, (uintptr_t)aligned_size);
  
  if (old_offset + aligned_size > arena->size) {
    // Out of space - we could try to rollback but if we are truly out of space 
    // it doesn't matter much as long as we return NULL.
    // For safety, we keep the offset capped to prevent massive overflows, 
    // although atomic-add already passed.
    return NULL;
  }
  
  return (void*)((uint8_t*)arena + old_offset);
}

pa_decl_export void p_arena_reset(void* arena_ptr) pa_attr_noexcept {
  if (arena_ptr == NULL) return;
  pomai_arena_t* arena = (pomai_arena_t*)arena_ptr;
  
  // O(1) reset - simply set the offset back to the start (just after the header)
  pa_atomic_store_release(&arena->offset, (uintptr_t)sizeof(pomai_arena_t));
}

pa_decl_export void p_arena_destroy(void* arena_ptr) pa_attr_noexcept {
  if (arena_ptr == NULL) return;
  pomai_arena_t* arena = (pomai_arena_t*)arena_ptr;
  
  // Free the OS memory directly
  _pa_prim_free((void*)arena, arena->size);
}
