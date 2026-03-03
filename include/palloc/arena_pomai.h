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

/** Opaque Vector Arena for PomaiDB/CheeseBrain: float[] embeddings, tensors, KV-caches. */
typedef struct pa_arena_s pa_arena_t;

/** SIMD alignment for vector ops (AVX2/AVX-512). Use 64 for best AVX-512. */
#define PA_ARENA_VECTOR_ALIGN  64

/**
 * Create a vector arena with a hard capacity limit (zero-OOM: never exceeds this).
 * Uses mmap/VirtualAlloc via palloc primitives; no general heap.
 * @param capacity_bytes  Maximum usable bytes for vector allocations (excluding header).
 * @return Arena pointer, or NULL on allocation failure.
 */
pa_decl_export pa_arena_t* p_arena_create_for_vector(size_t capacity_bytes) pa_attr_noexcept;

/**
 * Extended create: same as p_arena_create_for_vector but allows shared (multi-threaded) arena.
 * When shared is false, alloc/reset use no atomics for maximum single-thread throughput.
 * @param capacity_bytes  Maximum usable bytes for vector allocations (excluding header).
 * @param shared  If true, arena may be used from multiple threads (uses atomics).
 */
pa_decl_export pa_arena_t* p_arena_create_for_vector_ex(size_t capacity_bytes, bool shared) pa_attr_noexcept;

/**
 * O(1) bump allocation for one vector (e.g. float[vector_dim]).
 * Pointer is aligned to PA_ARENA_VECTOR_ALIGN for SIMD.
 * @return Pointer to aligned memory, or NULL if arena would exceed capacity.
 */
pa_decl_nodiscard pa_decl_export void* p_arena_alloc_vector(pa_arena_t* arena, size_t vector_dim, size_t element_size) pa_attr_noexcept;

/**
 * O(1) bulk reset: bump pointer set back to start. No per-block free.
 */
pa_decl_export void p_arena_reset(pa_arena_t* arena) pa_attr_noexcept;

/**
 * Release all arena memory to the OS.
 */
pa_decl_export void p_arena_destroy(pa_arena_t* arena) pa_attr_noexcept;

/* --- Legacy / compatibility (same behavior, byte-based) --- */
pa_decl_export void* p_arena_create(size_t size) pa_attr_noexcept;
pa_decl_export void* p_arena_alloc(void* arena, size_t size) pa_attr_noexcept;

#ifdef __cplusplus
}
#endif

#endif /* PALLOC_ARENA_POMAI_H */
