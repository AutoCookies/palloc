/*
 * Kernel-mode core API tests.
 * Built when PALLOC_BUILD_MODE=KERNEL. Uses pa_core_* and palloc_vector_* only.
 * No stdio/assert; freestanding. Run under PoOS or QEMU, or use as link check.
 */
#include <stddef.h>
#include <stdint.h>

#include "palloc_backend_api.h"
#include "palloc/core_types.h"
#include "palloc/core_api.h"

static void set_byte(void *p, size_t n, unsigned char val)
{
  unsigned char *q = (unsigned char *)p;
  while (n--)
    *q++ = val;
}

static int test_core_malloc_free(void)
{
  void *p = pa_core_malloc(64);
  if (!p) return 1;
  if (((uintptr_t)p % PALLOC_ALIGNMENT) != 0) return 1;
  set_byte(p, 64, 0xAB);
  pa_core_free(p);
  pa_core_free((void *)0);
  return 0;
}

static int test_core_malloc_aligned(void)
{
  void *p = pa_core_malloc_aligned(256, 64);
  if (!p) return 1;
  if (((uintptr_t)p % 64) != 0) return 1;
  pa_core_free(p);
  return 0;
}

static int test_core_vector(void)
{
  palloc_vector_t vec;
  palloc_vector_init(&vec);
  if (vec.capacity != 0 || vec.length != 0 || vec.data != (void *)0) return 1;

  if (palloc_vector_reserve(&vec, 4096) != 0) return 1;
  if (vec.capacity < 4096 || vec.data == (void *)0) return 1;
  if (((uintptr_t)vec.data % PALLOC_ALIGNMENT) != 0) return 1;

  palloc_vector_set_length(&vec, 128);
  if (vec.length != 128) return 1;

  palloc_vector_destroy(&vec);
  if (vec.data != (void *)0 || vec.capacity != 0) return 1;
  return 0;
}

static int test_backend_page_size(void)
{
  size_t ps = palloc_backend_page_size();
  if (ps < 4096 || ps > 65536) return 1;
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
