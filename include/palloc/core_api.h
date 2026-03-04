/* ----------------------------------------------------------------------------
 * Core allocator API: used only by core code (e.g. vector_core.c).
 * Pure C; depends only on core_types.h and palloc_backend_api.h.
 * -------------------------------------------------------------------------- */
#pragma once
#ifndef PALLOC_CORE_API_H
#define PALLOC_CORE_API_H

#include "core_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Allocate size bytes, 16-byte aligned. Returns NULL on failure. */
void *pa_core_malloc(size_t size);

/** Free pointer previously returned by pa_core_malloc. */
void pa_core_free(void *ptr);

/** Allocate size bytes with given alignment (>= PALLOC_ALIGNMENT). Returns NULL on failure. */
void *pa_core_malloc_aligned(size_t size, size_t alignment);

/* Vector API (fixed palloc_vector_t layout) */
void palloc_vector_init(palloc_vector_t *vec);
void palloc_vector_destroy(palloc_vector_t *vec);
int palloc_vector_reserve(palloc_vector_t *vec, size_t capacity_bytes);
void palloc_vector_set_length(palloc_vector_t *vec, size_t length_bytes);

#ifdef __cplusplus
}
#endif

#endif /* PALLOC_CORE_API_H */
