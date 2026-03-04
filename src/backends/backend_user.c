/* ----------------------------------------------------------------------------
 * User-mode backend: request/release pages via mmap/munmap.
 * For PomaiDB and other user-space consumers.
 * -------------------------------------------------------------------------- */
#include "palloc_backend_api.h"
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

static size_t s_page_size;

static size_t get_page_size(void)
{
  if (s_page_size == 0) {
    long ps = sysconf(_SC_PAGESIZE);
    s_page_size = (ps > 0) ? (size_t)ps : (size_t)PALLOC_PAGE_SIZE;
  }
  return s_page_size;
}

void palloc_backend_init(void)
{
  (void)get_page_size();
}

size_t palloc_backend_page_size(void)
{
  return get_page_size();
}

void *palloc_backend_request_pages(size_t num_pages)
{
  size_t ps = get_page_size();
  size_t size = num_pages * ps;
  void *p = mmap((void *)0, size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  return (p == MAP_FAILED) ? (void *)0 : p;
}

void palloc_backend_release_pages(void *ptr, size_t num_pages)
{
  if (!ptr)
    return;
  size_t size = num_pages * get_page_size();
  (void)munmap(ptr, size);
}

void palloc_backend_panic(const char *msg)
{
  (void)msg;
  abort();
}
