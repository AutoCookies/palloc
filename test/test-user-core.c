/*
 * User-mode core API tests.
 * Built and run when PALLOC_BUILD_MODE=USER. Uses pa_core_* and palloc_vector_* only.
 */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "palloc_backend_api.h"
#include "palloc/core_types.h"
#include "palloc/core_api.h"

#define ASSERT(c) do { if (!(c)) return 1; } while (0)

static int test_core_malloc_free(void)
{
  void *p = pa_core_malloc(64);
  ASSERT(p != NULL);
  ASSERT(((uintptr_t)p % PALLOC_ALIGNMENT) == 0);
  memset(p, 0xAB, 64);
  pa_core_free(p);
  pa_core_free(NULL);
  return 0;
}

static int test_core_malloc_aligned(void)
{
  void *p = pa_core_malloc_aligned(256, 64);
  ASSERT(p != NULL);
  ASSERT(((uintptr_t)p % 64) == 0);
  pa_core_free(p);
  return 0;
}

static int test_core_vector(void)
{
  palloc_vector_t vec;
  palloc_vector_init(&vec);
  ASSERT(vec.capacity == 0 && vec.length == 0 && vec.data == NULL);

  ASSERT(palloc_vector_reserve(&vec, 4096) == 0);
  ASSERT(vec.capacity >= 4096 && vec.data != NULL);
  ASSERT(((uintptr_t)vec.data % PALLOC_ALIGNMENT) == 0);

  palloc_vector_set_length(&vec, 128);
  ASSERT(vec.length == 128);

  palloc_vector_destroy(&vec);
  ASSERT(vec.data == NULL && vec.capacity == 0);
  return 0;
}

static int test_backend_page_size(void)
{
  size_t ps = palloc_backend_page_size();
  ASSERT(ps >= 4096 && ps <= 65536);
  return 0;
}

int main(void)
{
  palloc_backend_init();

  if (test_core_malloc_free() != 0) return 1;
  if (test_core_malloc_aligned() != 0) return 1;
  if (test_core_vector() != 0) return 1;
  if (test_backend_page_size() != 0) return 1;

  return 0;
}
