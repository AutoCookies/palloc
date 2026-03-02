/* ----------------------------------------------------------------------------
Copyright (c) 2018-2026, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#pragma once
#ifndef PALLOC_ARENA_POMAI_H
#define PALLOC_ARENA_POMAI_H

#include <stddef.h>
#include "palloc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates a large contiguous arena by bypassing normal caches.
 * 
 * Uses raw OS primitives (mmap/VirtualAlloc) to reserve a massive contiguous block.
 * 
 * @param size The total size of the arena to reserve.
 * @return void* Pointer to the arena object, or NULL on failure.
 */
pa_decl_export void* p_arena_create(size_t size) pa_attr_noexcept;

/**
 * @brief Allocates memory from the arena using a fast atomic bump-pointer.
 * 
 * @param arena The arena to allocate from.
 * @param size The size of the allocation.
 * @return void* Pointer to the allocated memory, or NULL if out of space.
 */
pa_decl_export void* p_arena_alloc(void* arena, size_t size) pa_attr_noexcept;

/**
 * @brief Instantly resets the arena's bump-pointer to 0. (O(1) reset)
 * 
 * @param arena The arena to reset.
 */
pa_decl_export void p_arena_reset(void* arena) pa_attr_noexcept;

/**
 * @brief Destroys the arena and releases all associated OS memory.
 * 
 * @param arena The arena to destroy.
 */
pa_decl_export void p_arena_destroy(void* arena) pa_attr_noexcept;

#ifdef __cplusplus
}
#endif

#endif // PALLOC_ARENA_POMAI_H
