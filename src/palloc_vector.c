/* ----------------------------------------------------------------------------
 * Vector-oriented allocator extension. Built when PA_VECTOR=ON.
 * Uses only public pa_* APIs (pa_heap_malloc, pa_heap_malloc_aligned, pa_free).
 * --------------------------------------------------------------------------*/
#include "palloc_vector.h"
#include "palloc.h"

#if defined(PA_VECTOR) && PA_VECTOR

#include <stddef.h>
#include <stdint.h>

#define PA_VEC_ALIGN_UP(sz, a)  (((sz) + (a) - 1) & ~((size_t)(a) - 1))

/* Chunk: one block from the heap; bump pointer in [base, base+capacity). */
typedef struct pa_vec_chunk_s {
  struct pa_vec_chunk_s* next;
  void*                  block;  /* from pa_heap_malloc, to free */
  void*                  base;   /* aligned payload start */
  size_t                 capacity;
  size_t                 used;
} pa_vec_chunk_t;

struct pa_vec_arena_s {
  pa_heap_t*       heap;
  size_t           alignment;
  size_t           max_size;    /* 0 = no cap */
  size_t           used_total;  /* sum of alloc sizes for cap check */
  size_t           chunk_size;  /* size for next new chunk */
  pa_vec_chunk_t*  chunks;
};

static void* alloc_chunk_block(pa_heap_t* heap, size_t chunk_size, size_t alignment, size_t* out_payload_offset) {
  size_t header_size = PA_VEC_ALIGN_UP(sizeof(pa_vec_chunk_t), alignment);
  size_t total = header_size + chunk_size;
  void* block = pa_heap_malloc_aligned(heap, total, alignment);
  if (!block) return NULL;
  *out_payload_offset = header_size;
  return block;
}

pa_vec_arena_t* pa_vec_arena_create(size_t initial_size, size_t alignment) pa_attr_noexcept {
  return pa_vec_arena_create_ex(initial_size, alignment, pa_heap_get_default());
}

pa_vec_arena_t* pa_vec_arena_create_ex(size_t initial_size, size_t alignment, pa_heap_t* heap) pa_attr_noexcept {
  if (heap == NULL || initial_size == 0 || alignment == 0) return NULL;
  if ((alignment & (alignment - 1)) != 0) return NULL; /* must be power of two */

  pa_vec_arena_t* arena = (pa_vec_arena_t*)pa_heap_malloc(heap, sizeof(pa_vec_arena_t));
  if (!arena) return NULL;

  arena->heap = heap;
  arena->alignment = alignment;
  arena->max_size = 0;
  arena->used_total = 0;
  arena->chunk_size = initial_size;
  arena->chunks = NULL;

  size_t payload_offset;
  void* block = alloc_chunk_block(heap, initial_size, alignment, &payload_offset);
  if (!block) {
    pa_free(arena);
    return NULL;
  }

  pa_vec_chunk_t* chunk = (pa_vec_chunk_t*)block;
  chunk->next = NULL;
  chunk->block = block;
  chunk->base = (char*)block + payload_offset;
  chunk->capacity = initial_size;
  chunk->used = 0;
  arena->chunks = chunk;

  return arena;
}

void* pa_vec_arena_alloc(pa_vec_arena_t* arena, size_t size) pa_attr_noexcept {
  if (arena == NULL || size == 0) return NULL;

  size_t aligned_size = PA_VEC_ALIGN_UP(size, arena->alignment);
  if (aligned_size < size) return NULL; /* overflow */

  if (arena->max_size != 0 && arena->used_total + aligned_size > arena->max_size)
    return NULL;

  pa_vec_chunk_t* chunk = arena->chunks;
  while (chunk != NULL) {
    if (chunk->used + aligned_size <= chunk->capacity) {
      void* p = (char*)chunk->base + chunk->used;
      chunk->used += aligned_size;
      arena->used_total += aligned_size;
      return p;
    }
    chunk = chunk->next;
  }

  /* Need new chunk: double size, at least aligned_size */
  size_t new_size = arena->chunk_size;
  while (new_size < aligned_size) new_size *= 2;
  arena->chunk_size = new_size * 2; /* next time double again */

  size_t payload_offset;
  void* block = alloc_chunk_block(arena->heap, new_size, arena->alignment, &payload_offset);
  if (!block) return NULL;

  pa_vec_chunk_t* new_chunk = (pa_vec_chunk_t*)block;
  new_chunk->next = arena->chunks;
  new_chunk->block = block;
  new_chunk->base = (char*)block + payload_offset;
  new_chunk->capacity = new_size;
  new_chunk->used = aligned_size;
  arena->chunks = new_chunk;
  arena->used_total += aligned_size;

  return new_chunk->base;
}

void pa_vec_arena_reset(pa_vec_arena_t* arena) pa_attr_noexcept {
  if (arena == NULL) return;
  pa_vec_chunk_t* chunk = arena->chunks;
  while (chunk != NULL) {
    chunk->used = 0;
    chunk = chunk->next;
  }
  arena->used_total = 0;
}

void pa_vec_arena_destroy(pa_vec_arena_t* arena) pa_attr_noexcept {
  if (arena == NULL) return;
  pa_vec_chunk_t* chunk = arena->chunks;
  while (chunk != NULL) {
    pa_vec_chunk_t* next = chunk->next;
    pa_free(chunk->block);
    chunk = next;
  }
  pa_free(arena);
}

void pa_vec_arena_set_max_size(pa_vec_arena_t* arena, size_t max_bytes) pa_attr_noexcept {
  if (arena != NULL) arena->max_size = max_bytes;
}

/* --------------------------------------------------------------------------
 * Batch allocation
 * ----------------------------------------------------------------------- */

void** pa_vector_batch_alloc(size_t count, size_t size) pa_attr_noexcept {
  if (count == 0) return NULL;
  pa_heap_t* heap = pa_heap_get_default();
  void** ptrs = (void**)pa_heap_malloc(heap, count * sizeof(void*));
  if (!ptrs) return NULL;
  for (size_t i = 0; i < count; i++) {
    ptrs[i] = pa_heap_malloc(heap, size);
    if (ptrs[i] == NULL) {
      while (i > 0) pa_free(ptrs[--i]);
      pa_free(ptrs);
      return NULL;
    }
  }
  return ptrs;
}

void pa_vector_batch_free(void** ptrs, size_t count) pa_attr_noexcept {
  if (ptrs == NULL) return;
  for (size_t i = 0; i < count; i++) pa_free(ptrs[i]);
  pa_free(ptrs);
}

float** pa_vector_batch_alloc_floats(size_t count, size_t dim) pa_attr_noexcept {
  if (count == 0) return NULL;
  size_t vec_size = dim * sizeof(float);
  pa_heap_t* heap = pa_heap_get_default();
  float** ptrs = (float**)pa_heap_malloc(heap, count * sizeof(float*));
  if (!ptrs) return NULL;
  for (size_t i = 0; i < count; i++) {
    ptrs[i] = (float*)pa_heap_malloc_aligned(heap, vec_size, PA_VECTOR_ALIGNMENT_DEFAULT);
    if (ptrs[i] == NULL) {
      while (i > 0) pa_free(ptrs[--i]);
      pa_free(ptrs);
      return NULL;
    }
  }
  return ptrs;
}

/* --------------------------------------------------------------------------
 * Fixed-size object pool
 * ----------------------------------------------------------------------- */

struct pa_vec_pool_s {
  pa_heap_t*  heap;
  size_t      object_size;
  void*       free_list;   /* linked list of free objects (next ptr in first word) */
  void*       pages;      /* linked list of page blocks to free on destroy */
};

pa_vec_pool_t* pa_vec_pool_create(size_t object_size, size_t initial_count) pa_attr_noexcept {
  if (object_size == 0) return NULL;
  pa_heap_t* heap = pa_heap_get_default();

  pa_vec_pool_t* pool = (pa_vec_pool_t*)pa_heap_malloc(heap, sizeof(pa_vec_pool_t));
  if (!pool) return NULL;

  pool->heap = heap;
  pool->object_size = object_size;
  pool->free_list = NULL;
  pool->pages = NULL;

  if (initial_count > 0) {
    size_t page_size = object_size * initial_count;
    void* page = pa_heap_malloc(heap, page_size);
    if (!page) {
      pa_free(pool);
      return NULL;
    }
    *(void**)page = pool->pages;
    pool->pages = page;

    char* p = (char*)page;
    for (size_t i = 0; i < initial_count; i++) {
      *(void**)p = pool->free_list;
      pool->free_list = p;
      p += object_size;
    }
  }

  return pool;
}

void* pa_vec_pool_alloc(pa_vec_pool_t* pool) pa_attr_noexcept {
  if (pool == NULL) return NULL;

  if (pool->free_list != NULL) {
    void* p = pool->free_list;
    pool->free_list = *(void**)p;
    return p;
  }

  /* Allocate new page: one block of object_size * 64 or similar */
  size_t count = 64;
  size_t page_size = pool->object_size * count;
  void* page = pa_heap_malloc(pool->heap, page_size);
  if (!page) return NULL;

  *(void**)page = pool->pages;
  pool->pages = page;

  char* p = (char*)page;
  for (size_t i = 0; i < count; i++) {
    *(void**)p = pool->free_list;
    pool->free_list = p;
    p += pool->object_size;
  }

  void* result = pool->free_list;
  pool->free_list = *(void**)result;
  return result;
}

void pa_vec_pool_free(pa_vec_pool_t* pool, void* ptr) pa_attr_noexcept {
  if (pool == NULL || ptr == NULL) return;
  *(void**)ptr = pool->free_list;
  pool->free_list = ptr;
}

void pa_vec_pool_destroy(pa_vec_pool_t* pool) pa_attr_noexcept {
  if (pool == NULL) return;
  void* page = pool->pages;
  while (page != NULL) {
    void* next = *(void**)page;
    pa_free(page);
    page = next;
  }
  pa_free(pool);
}

#endif /* PA_VECTOR */
