/*
 * Kernel-style backend for running on host (e.g. CI, dev).
 * Same API as backend_kernel.c but uses mmap so bench-kernel-core can run
 * without segfault. Use -DPALLOC_KERNEL_RUN_ON_HOST=ON when building KERNEL mode.
 */
#include "palloc_backend_api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#define PALLOC_KERNEL_PHYS_SIZE (256u * 1024u * 1024u)

static size_t s_page_size;
static uintptr_t s_next;
static uintptr_t s_base;

void palloc_backend_init(void)
{
  if (s_page_size == 0) {
    long ps = sysconf(_SC_PAGESIZE);
    s_page_size = (ps > 0) ? (size_t)ps : (size_t)PALLOC_PAGE_SIZE;
  }
  if (s_base == 0) {
    void *p = mmap((void *)0, PALLOC_KERNEL_PHYS_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED)
      p = (void *)0;
    s_base = (uintptr_t)p;
    s_next = s_base;
  }
}

size_t palloc_backend_page_size(void)
{
  return s_page_size ? s_page_size : (size_t)PALLOC_PAGE_SIZE;
}

void *palloc_backend_request_pages(size_t num_pages)
{
  size_t ps = palloc_backend_page_size();
  size_t size = num_pages * ps;
  uintptr_t end = s_next + size;
  if (s_base == 0 || end > s_base + PALLOC_KERNEL_PHYS_SIZE)
    return (void *)0;
  void *ptr = (void *)s_next;
  s_next = end;
  return ptr;
}

void palloc_backend_release_pages(void *ptr, size_t num_pages)
{
  (void)ptr;
  (void)num_pages;
}

void palloc_backend_panic(const char *msg)
{
  (void)msg;
  abort();
}
