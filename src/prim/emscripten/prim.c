/* ----------------------------------------------------------------------------
Copyright (c) 2018-2025, Microsoft Research, Daan Leijen, Alon Zakai
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

// This file is included in `src/prim/prim.c`

#include "palloc.h"
#include "palloc/internal.h"
#include "palloc/atomic.h"
#include "palloc/prim.h"

// Design
// ======
//
// palloc is built on top of emmalloc. emmalloc is a minimal allocator on top
// of sbrk. The reason for having three layers here is that we want palloc to
// be able to allocate and release system memory properly, the same way it would
// when using VirtualAlloc on Windows or mmap on POSIX, and sbrk is too limited.
// Specifically, sbrk can only go up and down, and not "skip" over regions, and
// so we end up either never freeing memory to the system, or we can get stuck
// with holes.
//
// Atm wasm generally does *not* free memory back the system: once grown, we do
// not shrink back down (https://github.com/WebAssembly/design/issues/1397).
// However, that is expected to improve
// (https://github.com/WebAssembly/memory-control/blob/main/proposals/memory-control/Overview.md)
// and so we do not want to bake those limitations in here.
//
// Even without that issue, we want our system allocator to handle holes, that
// is, it should merge freed regions and allow allocating new content there of
// the full size, etc., so that we do not waste space. That means that the
// system allocator really does need to handle the general problem of allocating
// and freeing variable-sized chunks of memory in a random order, like malloc/
// free do. And so it makes sense to layer palloc on top of such an
// implementation.
//
// emmalloc makes sense for the lower level because it is small and simple while
// still fully handling merging of holes etc. It is not the most efficient
// allocator, but our assumption is that palloc needs to be fast while the
// system allocator underneath it is called much less frequently.
//

//---------------------------------------------
// init
//---------------------------------------------

void _pa_prim_mem_init( pa_os_mem_config_t* config) {
  config->page_size = 64*PA_KiB; // WebAssembly has a fixed page size: 64KiB
  config->alloc_granularity = 16;
  config->has_overcommit = false;
  config->has_partial_free = false;
  config->has_virtual_reserve = false;
}

extern void emmalloc_free(void*);

int _pa_prim_free(void* addr, size_t size) {
  if (size==0) return 0;
  emmalloc_free(addr);
  return 0;
}


//---------------------------------------------
// Allocation
//---------------------------------------------

extern void* emmalloc_memalign(size_t alignment, size_t size);

// Note: the `try_alignment` is just a hint and the returned pointer is not guaranteed to be aligned.
int _pa_prim_alloc(void* hint_addr, size_t size, size_t try_alignment, bool commit, bool allow_large, bool* is_large, bool* is_zero, void** addr) {
  PA_UNUSED(try_alignment); PA_UNUSED(allow_large); PA_UNUSED(commit); PA_UNUSED(hint_addr);
  *is_large = false;
  // TODO: Track the highest address ever seen; first uses of it are zeroes.
  //       That assumes no one else uses sbrk but us (they could go up,
  //       scribble, and then down), but we could assert on that perhaps.
  *is_zero = false;
  // emmalloc has a minimum alignment size.
  #define MIN_EMMALLOC_ALIGN           8
  if (try_alignment < MIN_EMMALLOC_ALIGN) {
    try_alignment = MIN_EMMALLOC_ALIGN;
  }
  void* p = emmalloc_memalign(try_alignment, size);
  *addr = p;
  if (p == 0) {
    return ENOMEM;
  }
  return 0;
}


//---------------------------------------------
// Commit/Reset
//---------------------------------------------

int _pa_prim_commit(void* addr, size_t size, bool* is_zero) {
  PA_UNUSED(addr); PA_UNUSED(size);
  // See TODO above.
  *is_zero = false;
  return 0;
}

int _pa_prim_decommit(void* addr, size_t size, bool* needs_recommit) {
  PA_UNUSED(addr); PA_UNUSED(size);
  *needs_recommit = false;
  return 0;
}

int _pa_prim_reset(void* addr, size_t size) {
  PA_UNUSED(addr); PA_UNUSED(size);
  return 0;
}

int _pa_prim_reuse(void* addr, size_t size) {
  PA_UNUSED(addr); PA_UNUSED(size);
  return 0;
}

int _pa_prim_protect(void* addr, size_t size, bool protect) {
  PA_UNUSED(addr); PA_UNUSED(size); PA_UNUSED(protect);
  return 0;
}


//---------------------------------------------
// Huge pages and NUMA nodes
//---------------------------------------------

int _pa_prim_alloc_huge_os_pages(void* hint_addr, size_t size, int numa_node, bool* is_zero, void** addr) {
  PA_UNUSED(hint_addr); PA_UNUSED(size); PA_UNUSED(numa_node);
  *is_zero = true;
  *addr = NULL;
  return ENOSYS;
}

size_t _pa_prim_numa_node(void) {
  return 0;
}

size_t _pa_prim_numa_node_count(void) {
  return 1;
}


//----------------------------------------------------------------
// Clock
//----------------------------------------------------------------

#include <emscripten/html5.h>

pa_msecs_t _pa_prim_clock_now(void) {
  return emscripten_date_now();
}


//----------------------------------------------------------------
// Process info
//----------------------------------------------------------------

void _pa_prim_process_info(pa_process_info_t* pinfo)
{
  // use defaults
  PA_UNUSED(pinfo);
}


//----------------------------------------------------------------
// Output
//----------------------------------------------------------------

#include <emscripten/console.h>

void _pa_prim_out_stderr( const char* msg) {
  emscripten_console_error(msg);
}


//----------------------------------------------------------------
// Environment
//----------------------------------------------------------------

bool _pa_prim_getenv(const char* name, char* result, size_t result_size) {
  // For code size reasons, do not support environ customization for now.
  PA_UNUSED(name);
  PA_UNUSED(result);
  PA_UNUSED(result_size);
  return false;
}


//----------------------------------------------------------------
// Random
//----------------------------------------------------------------

bool _pa_prim_random_buf(void* buf, size_t buf_len) {
  int err = getentropy(buf, buf_len);
  return !err;
}


//----------------------------------------------------------------
// Thread init/done
//----------------------------------------------------------------

#if defined(PA_USE_PTHREADS)

// use pthread local storage keys to detect thread ending
// (and used with PA_TLS_PTHREADS for the default heap)
pthread_key_t _pa_heap_default_key = (pthread_key_t)(-1);

static void pa_pthread_done(void* value) {
  if (value!=NULL) {
    _pa_thread_done((pa_heap_t*)value);
  }
}

void _pa_prim_thread_init_auto_done(void) {
  pa_assert_internal(_pa_heap_default_key == (pthread_key_t)(-1));
  pthread_key_create(&_pa_heap_default_key, &pa_pthread_done);
}

void _pa_prim_thread_done_auto_done(void) {
  // nothing to do
}

void _pa_prim_thread_associate_default_heap(pa_heap_t* heap) {
  if (_pa_heap_default_key != (pthread_key_t)(-1)) {  // can happen during recursive invocation on freeBSD
    pthread_setspecific(_pa_heap_default_key, heap);
  }
}

#else

void _pa_prim_thread_init_auto_done(void) {
  // nothing
}

void _pa_prim_thread_done_auto_done(void) {
  // nothing
}

void _pa_prim_thread_associate_default_heap(pa_heap_t* heap) {
  PA_UNUSED(heap);

}
#endif
