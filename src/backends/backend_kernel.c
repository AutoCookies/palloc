/* ----------------------------------------------------------------------------
 * Kernel-mode backend: physical memory (Allwinner H3 placeholder).
 * For PoOS; no libc. Flat region at 0x40000000.
 * -------------------------------------------------------------------------- */
#include "palloc_backend_api.h"
#include <stddef.h>
#include <stdint.h>

/* Allwinner H3: typical RAM start (adjust to match PoOS memory map). */
#define PALLOC_KERNEL_PHYS_BASE  0x40000000u
#define PALLOC_KERNEL_PHYS_SIZE (256u * 1024u * 1024u)  /* 256 MiB placeholder */

static uintptr_t s_next;

void palloc_backend_init(void)
{
  s_next = PALLOC_KERNEL_PHYS_BASE;
}

size_t palloc_backend_page_size(void)
{
  return (size_t)PALLOC_PAGE_SIZE;
}

void *palloc_backend_request_pages(size_t num_pages)
{
  size_t size = num_pages * (size_t)PALLOC_PAGE_SIZE;
  uintptr_t end = s_next + size;
  if (end > PALLOC_KERNEL_PHYS_BASE + PALLOC_KERNEL_PHYS_SIZE)
    return (void *)0;
  void *ptr = (void *)s_next;
  s_next = end;
  return ptr;
}

void palloc_backend_release_pages(void *ptr, size_t num_pages)
{
  (void)ptr;
  (void)num_pages;
  /* Placeholder: no-op. Full impl would return pages to physical allocator. */
}

void palloc_backend_panic(const char *msg)
{
  (void)msg;
  /* Halt: replace with PoOS kernel panic when integrated. */
  for (;;)
    ;
}
