/* ----------------------------------------------------------------------------
Copyright (c) 2018-2023, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#pragma once
#ifndef PALLOC_TRACK_H
#define PALLOC_TRACK_H

/* ------------------------------------------------------------------------------------------------------
Track memory ranges with macros for tools like Valgrind address sanitizer, or other memory checkers.
These can be defined for tracking allocation:

  #define pa_track_malloc_size(p,reqsize,size,zero)
  #define pa_track_free_size(p,_size)

The macros are set up such that the size passed to `pa_track_free_size`
always matches the size of `pa_track_malloc_size`. (currently, `size == pa_usable_size(p)`).
The `reqsize` is what the user requested, and `size >= reqsize`.
The `size` is either byte precise (and `size==reqsize`) if `PA_PADDING` is enabled,
or otherwise it is the usable block size which may be larger than the original request.
Use `_pa_block_size_of(void* p)` to get the full block size that was allocated (including padding etc).
The `zero` parameter is `true` if the allocated block is zero initialized.

Optional:

  #define pa_track_align(p,alignedp,offset,size)
  #define pa_track_resize(p,oldsize,newsize)
  #define pa_track_init()

The `pa_track_align` is called right after a `pa_track_malloc` for aligned pointers in a block.
The corresponding `pa_track_free` still uses the block start pointer and original size (corresponding to the `pa_track_malloc`).
The `pa_track_resize` is currently unused but could be called on reallocations within a block.
`pa_track_init` is called at program start.

The following macros are for tools like asan and valgrind to track whether memory is
defined, undefined, or not accessible at all:

  #define pa_track_mem_defined(p,size)
  #define pa_track_mem_undefined(p,size)
  #define pa_track_mem_noaccess(p,size)

-------------------------------------------------------------------------------------------------------*/

#if PA_TRACK_VALGRIND
// valgrind tool

#define PA_TRACK_ENABLED      1
#define PA_TRACK_HEAP_DESTROY 1           // track free of individual blocks on heap_destroy
#define PA_TRACK_TOOL         "valgrind"

#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>

#define pa_track_malloc_size(p,reqsize,size,zero) VALGRIND_MALLOCLIKE_BLOCK(p,size,PA_PADDING_SIZE /*red zone*/,zero)
#define pa_track_free_size(p,_size)               VALGRIND_FREELIKE_BLOCK(p,PA_PADDING_SIZE /*red zone*/)
#define pa_track_resize(p,oldsize,newsize)        VALGRIND_RESIZEINPLACE_BLOCK(p,oldsize,newsize,PA_PADDING_SIZE /*red zone*/)
#define pa_track_mem_defined(p,size)              VALGRIND_MAKE_MEM_DEFINED(p,size)
#define pa_track_mem_undefined(p,size)            VALGRIND_MAKE_MEM_UNDEFINED(p,size)
#define pa_track_mem_noaccess(p,size)             VALGRIND_MAKE_MEM_NOACCESS(p,size)

#elif PA_TRACK_ASAN
// address sanitizer

#define PA_TRACK_ENABLED      1
#define PA_TRACK_HEAP_DESTROY 0
#define PA_TRACK_TOOL         "asan"

#include <sanitizer/asan_interface.h>

#define pa_track_malloc_size(p,reqsize,size,zero) ASAN_UNPOISON_MEMORY_REGION(p,size)
#define pa_track_free_size(p,size)                ASAN_POISON_MEMORY_REGION(p,size)
#define pa_track_mem_defined(p,size)              ASAN_UNPOISON_MEMORY_REGION(p,size)
#define pa_track_mem_undefined(p,size)            ASAN_UNPOISON_MEMORY_REGION(p,size)
#define pa_track_mem_noaccess(p,size)             ASAN_POISON_MEMORY_REGION(p,size)

#elif PA_TRACK_ETW
// windows event tracing

#define PA_TRACK_ENABLED      1
#define PA_TRACK_HEAP_DESTROY 1
#define PA_TRACK_TOOL         "ETW"

#include "../src/prim/windows/etw.h"

#define pa_track_init()                           EventRegistermicrosoft_windows_palloc();
#define pa_track_malloc_size(p,reqsize,size,zero) EventWriteETW_PA_ALLOC((UINT64)(p), size)
#define pa_track_free_size(p,size)                EventWriteETW_PA_FREE((UINT64)(p), size)

#else
// no tracking

#define PA_TRACK_ENABLED      0
#define PA_TRACK_HEAP_DESTROY 0
#define PA_TRACK_TOOL         "none"

#define pa_track_malloc_size(p,reqsize,size,zero)
#define pa_track_free_size(p,_size)

#endif

// -------------------
// Utility definitions

#ifndef pa_track_resize
#define pa_track_resize(p,oldsize,newsize)      pa_track_free_size(p,oldsize); pa_track_malloc(p,newsize,false)
#endif

#ifndef pa_track_align
#define pa_track_align(p,alignedp,offset,size)  pa_track_mem_noaccess(p,offset)
#endif

#ifndef pa_track_init
#define pa_track_init()
#endif

#ifndef pa_track_mem_defined
#define pa_track_mem_defined(p,size)
#endif

#ifndef pa_track_mem_undefined
#define pa_track_mem_undefined(p,size)
#endif

#ifndef pa_track_mem_noaccess
#define pa_track_mem_noaccess(p,size)
#endif


#if PA_PADDING
#define pa_track_malloc(p,reqsize,zero) \
  if ((p)!=NULL) { \
    pa_assert_internal(pa_usable_size(p)==(reqsize)); \
    pa_track_malloc_size(p,reqsize,reqsize,zero); \
  }
#else
#define pa_track_malloc(p,reqsize,zero) \
  if ((p)!=NULL) { \
    pa_assert_internal(pa_usable_size(p)>=(reqsize)); \
    pa_track_malloc_size(p,reqsize,pa_usable_size(p),zero); \
  }
#endif

#endif
