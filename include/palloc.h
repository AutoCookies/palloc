/* ----------------------------------------------------------------------------
Copyright (c) 2018-2026, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#pragma once
#ifndef PALLOC_H
#define PALLOC_H

#define PA_MALLOC_VERSION 227  // major + 2 digits minor

// ------------------------------------------------------
// Compiler specific attributes
// ------------------------------------------------------

#ifdef __cplusplus
  #if (__cplusplus >= 201103L) || (_MSC_VER > 1900)  // C++11
    #define pa_attr_noexcept   noexcept
  #else
    #define pa_attr_noexcept   throw()
  #endif
#else
  #define pa_attr_noexcept
#endif

#if defined(__cplusplus) && (__cplusplus >= 201703)
  #define pa_decl_nodiscard    [[nodiscard]]
#elif (defined(__GNUC__) && (__GNUC__ >= 4)) || defined(__clang__)  // includes clang, icc, and clang-cl
  #define pa_decl_nodiscard    __attribute__((warn_unused_result))
#elif defined(_HAS_NODISCARD)
  #define pa_decl_nodiscard    _NODISCARD
#elif (_MSC_VER >= 1700)
  #define pa_decl_nodiscard    _Check_return_
#else
  #define pa_decl_nodiscard
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
  #if !defined(PA_SHARED_LIB)
    #define pa_decl_export
  #elif defined(PA_SHARED_LIB_EXPORT)
    #define pa_decl_export              __declspec(dllexport)
  #else
    #define pa_decl_export              __declspec(dllimport)
  #endif
  #if defined(__MINGW32__)
    #define pa_decl_restrict
    #define pa_attr_malloc              __attribute__((malloc))
  #else
    #if (_MSC_VER >= 1900) && !defined(__EDG__)
      #define pa_decl_restrict          __declspec(allocator) __declspec(restrict)
    #else
      #define pa_decl_restrict          __declspec(restrict)
    #endif
    #define pa_attr_malloc
  #endif
  #define pa_cdecl                      __cdecl
  #define pa_attr_alloc_size(s)
  #define pa_attr_alloc_size2(s1,s2)
  #define pa_attr_alloc_align(p)
#elif defined(__GNUC__)                 // includes clang and icc
  #if defined(PA_SHARED_LIB) && defined(PA_SHARED_LIB_EXPORT)
    #define pa_decl_export              __attribute__((visibility("default")))
  #else
    #define pa_decl_export
  #endif
  #define pa_cdecl                      // leads to warnings... __attribute__((cdecl))
  #define pa_decl_restrict
  #define pa_attr_malloc                __attribute__((malloc))
  #if (defined(__clang_major__) && (__clang_major__ < 4)) || (__GNUC__ < 5)
    #define pa_attr_alloc_size(s)
    #define pa_attr_alloc_size2(s1,s2)
    #define pa_attr_alloc_align(p)
  #elif defined(__INTEL_COMPILER)
    #define pa_attr_alloc_size(s)       __attribute__((alloc_size(s)))
    #define pa_attr_alloc_size2(s1,s2)  __attribute__((alloc_size(s1,s2)))
    #define pa_attr_alloc_align(p)
  #else
    #define pa_attr_alloc_size(s)       __attribute__((alloc_size(s)))
    #define pa_attr_alloc_size2(s1,s2)  __attribute__((alloc_size(s1,s2)))
    #define pa_attr_alloc_align(p)      __attribute__((alloc_align(p)))
  #endif
#else
  #define pa_cdecl
  #define pa_decl_export
  #define pa_decl_restrict
  #define pa_attr_malloc
  #define pa_attr_alloc_size(s)
  #define pa_attr_alloc_size2(s1,s2)
  #define pa_attr_alloc_align(p)
#endif

// ------------------------------------------------------
// Includes
// ------------------------------------------------------

#include <stddef.h>     // size_t
#include <stdbool.h>    // bool
#include <stdint.h>     // INTPTR_MAX

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------------------------------------
// Standard malloc interface
// ------------------------------------------------------

pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_malloc(size_t size)  pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_calloc(size_t count, size_t size)  pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size2(1,2);
pa_decl_nodiscard pa_decl_export void* pa_realloc(void* p, size_t newsize)      pa_attr_noexcept pa_attr_alloc_size(2);
pa_decl_export void* pa_expand(void* p, size_t newsize)                         pa_attr_noexcept pa_attr_alloc_size(2);

pa_decl_export void pa_free(void* p) pa_attr_noexcept;
pa_decl_nodiscard pa_decl_export pa_decl_restrict char* pa_strdup(const char* s) pa_attr_noexcept pa_attr_malloc;
pa_decl_nodiscard pa_decl_export pa_decl_restrict char* pa_strndup(const char* s, size_t n) pa_attr_noexcept pa_attr_malloc;
pa_decl_nodiscard pa_decl_export pa_decl_restrict char* pa_realpath(const char* fname, char* resolved_name) pa_attr_noexcept pa_attr_malloc;

// ------------------------------------------------------
// Extended functionality
// ------------------------------------------------------
#define PA_SMALL_WSIZE_MAX  (128)
#define PA_SMALL_SIZE_MAX   (PA_SMALL_WSIZE_MAX*sizeof(void*))

pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_malloc_small(size_t size) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_zalloc_small(size_t size) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_zalloc(size_t size)       pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1);

pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_mallocn(size_t count, size_t size) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size2(1,2);
pa_decl_nodiscard pa_decl_export void* pa_reallocn(void* p, size_t count, size_t size)        pa_attr_noexcept pa_attr_alloc_size2(2,3);
pa_decl_nodiscard pa_decl_export void* pa_reallocf(void* p, size_t newsize)                   pa_attr_noexcept pa_attr_alloc_size(2);

pa_decl_nodiscard pa_decl_export size_t pa_usable_size(const void* p) pa_attr_noexcept;
pa_decl_nodiscard pa_decl_export size_t pa_good_size(size_t size)     pa_attr_noexcept;


// ------------------------------------------------------
// Internals
// ------------------------------------------------------

typedef void (pa_cdecl pa_deferred_free_fun)(bool force, unsigned long long heartbeat, void* arg);
pa_decl_export void pa_register_deferred_free(pa_deferred_free_fun* deferred_free, void* arg) pa_attr_noexcept;

typedef void (pa_cdecl pa_output_fun)(const char* msg, void* arg);
pa_decl_export void pa_register_output(pa_output_fun* out, void* arg) pa_attr_noexcept;

typedef void (pa_cdecl pa_error_fun)(int err, void* arg);
pa_decl_export void pa_register_error(pa_error_fun* fun, void* arg);

pa_decl_export void pa_collect(bool force)    pa_attr_noexcept;
pa_decl_export int  pa_version(void)          pa_attr_noexcept;
pa_decl_export void pa_stats_reset(void)      pa_attr_noexcept;
pa_decl_export void pa_stats_merge(void)      pa_attr_noexcept;
pa_decl_export void pa_stats_print(void* out) pa_attr_noexcept;  // backward compatibility: `out` is ignored and should be NULL
pa_decl_export void pa_stats_print_out(pa_output_fun* out, void* arg) pa_attr_noexcept;
pa_decl_export void pa_thread_stats_print_out(pa_output_fun* out, void* arg) pa_attr_noexcept;
pa_decl_export void pa_options_print(void)    pa_attr_noexcept;

pa_decl_export void pa_process_info(size_t* elapsed_msecs, size_t* user_msecs, size_t* system_msecs,
                                    size_t* current_rss, size_t* peak_rss,
                                    size_t* current_commit, size_t* peak_commit, size_t* page_faults) pa_attr_noexcept;


// Generally do not use the following as these are usually called automatically
pa_decl_export void pa_process_init(void)     pa_attr_noexcept;
pa_decl_export void pa_cdecl pa_process_done(void) pa_attr_noexcept;
pa_decl_export void pa_thread_init(void)      pa_attr_noexcept;
pa_decl_export void pa_thread_done(void)      pa_attr_noexcept;


// -------------------------------------------------------------------------------------
// Aligned allocation
// Note that `alignment` always follows `size` for consistency with unaligned
// allocation, but unfortunately this differs from `posix_memalign` and `aligned_alloc`.
// -------------------------------------------------------------------------------------

pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_malloc_aligned(size_t size, size_t alignment) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1) pa_attr_alloc_align(2);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_malloc_aligned_at(size_t size, size_t alignment, size_t offset) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_zalloc_aligned(size_t size, size_t alignment) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1) pa_attr_alloc_align(2);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_zalloc_aligned_at(size_t size, size_t alignment, size_t offset) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_calloc_aligned(size_t count, size_t size, size_t alignment) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size2(1,2) pa_attr_alloc_align(3);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_calloc_aligned_at(size_t count, size_t size, size_t alignment, size_t offset) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size2(1,2);
pa_decl_nodiscard pa_decl_export void* pa_realloc_aligned(void* p, size_t newsize, size_t alignment) pa_attr_noexcept pa_attr_alloc_size(2) pa_attr_alloc_align(3);
pa_decl_nodiscard pa_decl_export void* pa_realloc_aligned_at(void* p, size_t newsize, size_t alignment, size_t offset) pa_attr_noexcept pa_attr_alloc_size(2);


// -----------------------------------------------------------------
// Return allocated block size (if the return value is not NULL)
// -----------------------------------------------------------------

pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_umalloc(size_t size, size_t* block_size)  pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_ucalloc(size_t count, size_t size, size_t* block_size)  pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size2(1,2);
pa_decl_nodiscard pa_decl_export void* pa_urealloc(void* p, size_t newsize, size_t* block_size_pre, size_t* block_size_post) pa_attr_noexcept pa_attr_alloc_size(2);
pa_decl_export void pa_ufree(void* p, size_t* block_size) pa_attr_noexcept;

pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_umalloc_aligned(size_t size, size_t alignment, size_t* block_size) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1) pa_attr_alloc_align(2);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_uzalloc_aligned(size_t size, size_t alignment, size_t* block_size) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1) pa_attr_alloc_align(2);

pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_umalloc_small(size_t size, size_t* block_size) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_uzalloc_small(size_t size, size_t* block_size) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1);


// -------------------------------------------------------------------------------------
// Heaps: first-class, but can only allocate from the same thread that created it.
// -------------------------------------------------------------------------------------

struct pa_heap_s;
typedef struct pa_heap_s pa_heap_t;

pa_decl_nodiscard pa_decl_export pa_heap_t* pa_heap_new(void);
pa_decl_export void       pa_heap_delete(pa_heap_t* heap);
pa_decl_export void       pa_heap_destroy(pa_heap_t* heap);
pa_decl_export pa_heap_t* pa_heap_set_default(pa_heap_t* heap);
pa_decl_export pa_heap_t* pa_heap_get_default(void);
pa_decl_export pa_heap_t* pa_heap_get_backing(void);
pa_decl_export void       pa_heap_collect(pa_heap_t* heap, bool force) pa_attr_noexcept;

pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_heap_malloc(pa_heap_t* heap, size_t size) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(2);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_heap_zalloc(pa_heap_t* heap, size_t size) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(2);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_heap_calloc(pa_heap_t* heap, size_t count, size_t size) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size2(2, 3);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_heap_mallocn(pa_heap_t* heap, size_t count, size_t size) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size2(2, 3);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_heap_malloc_small(pa_heap_t* heap, size_t size) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(2);

pa_decl_nodiscard pa_decl_export void* pa_heap_realloc(pa_heap_t* heap, void* p, size_t newsize)              pa_attr_noexcept pa_attr_alloc_size(3);
pa_decl_nodiscard pa_decl_export void* pa_heap_reallocn(pa_heap_t* heap, void* p, size_t count, size_t size)  pa_attr_noexcept pa_attr_alloc_size2(3,4);
pa_decl_nodiscard pa_decl_export void* pa_heap_reallocf(pa_heap_t* heap, void* p, size_t newsize)             pa_attr_noexcept pa_attr_alloc_size(3);

pa_decl_nodiscard pa_decl_export pa_decl_restrict char* pa_heap_strdup(pa_heap_t* heap, const char* s)            pa_attr_noexcept pa_attr_malloc;
pa_decl_nodiscard pa_decl_export pa_decl_restrict char* pa_heap_strndup(pa_heap_t* heap, const char* s, size_t n) pa_attr_noexcept pa_attr_malloc;
pa_decl_nodiscard pa_decl_export pa_decl_restrict char* pa_heap_realpath(pa_heap_t* heap, const char* fname, char* resolved_name) pa_attr_noexcept pa_attr_malloc;

pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_heap_malloc_aligned(pa_heap_t* heap, size_t size, size_t alignment) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(2) pa_attr_alloc_align(3);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_heap_malloc_aligned_at(pa_heap_t* heap, size_t size, size_t alignment, size_t offset) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(2);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_heap_zalloc_aligned(pa_heap_t* heap, size_t size, size_t alignment) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(2) pa_attr_alloc_align(3);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_heap_zalloc_aligned_at(pa_heap_t* heap, size_t size, size_t alignment, size_t offset) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(2);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_heap_calloc_aligned(pa_heap_t* heap, size_t count, size_t size, size_t alignment) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size2(2, 3) pa_attr_alloc_align(4);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_heap_calloc_aligned_at(pa_heap_t* heap, size_t count, size_t size, size_t alignment, size_t offset) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size2(2, 3);
pa_decl_nodiscard pa_decl_export void* pa_heap_realloc_aligned(pa_heap_t* heap, void* p, size_t newsize, size_t alignment) pa_attr_noexcept pa_attr_alloc_size(3) pa_attr_alloc_align(4);
pa_decl_nodiscard pa_decl_export void* pa_heap_realloc_aligned_at(pa_heap_t* heap, void* p, size_t newsize, size_t alignment, size_t offset) pa_attr_noexcept pa_attr_alloc_size(3);


// --------------------------------------------------------------------------------
// Zero initialized re-allocation.
// Only valid on memory that was originally allocated with zero initialization too.
// e.g. `pa_calloc`, `pa_zalloc`, `pa_zalloc_aligned` etc.
// see <https://github.com/microsoft/palloc/issues/63#issuecomment-508272992>
// --------------------------------------------------------------------------------

pa_decl_nodiscard pa_decl_export void* pa_rezalloc(void* p, size_t newsize)                pa_attr_noexcept pa_attr_alloc_size(2);
pa_decl_nodiscard pa_decl_export void* pa_recalloc(void* p, size_t newcount, size_t size)  pa_attr_noexcept pa_attr_alloc_size2(2,3);

pa_decl_nodiscard pa_decl_export void* pa_rezalloc_aligned(void* p, size_t newsize, size_t alignment) pa_attr_noexcept pa_attr_alloc_size(2) pa_attr_alloc_align(3);
pa_decl_nodiscard pa_decl_export void* pa_rezalloc_aligned_at(void* p, size_t newsize, size_t alignment, size_t offset) pa_attr_noexcept pa_attr_alloc_size(2);
pa_decl_nodiscard pa_decl_export void* pa_recalloc_aligned(void* p, size_t newcount, size_t size, size_t alignment) pa_attr_noexcept pa_attr_alloc_size2(2,3) pa_attr_alloc_align(4);
pa_decl_nodiscard pa_decl_export void* pa_recalloc_aligned_at(void* p, size_t newcount, size_t size, size_t alignment, size_t offset) pa_attr_noexcept pa_attr_alloc_size2(2,3);

pa_decl_nodiscard pa_decl_export void* pa_heap_rezalloc(pa_heap_t* heap, void* p, size_t newsize)                pa_attr_noexcept pa_attr_alloc_size(3);
pa_decl_nodiscard pa_decl_export void* pa_heap_recalloc(pa_heap_t* heap, void* p, size_t newcount, size_t size)  pa_attr_noexcept pa_attr_alloc_size2(3,4);

pa_decl_nodiscard pa_decl_export void* pa_heap_rezalloc_aligned(pa_heap_t* heap, void* p, size_t newsize, size_t alignment) pa_attr_noexcept pa_attr_alloc_size(3) pa_attr_alloc_align(4);
pa_decl_nodiscard pa_decl_export void* pa_heap_rezalloc_aligned_at(pa_heap_t* heap, void* p, size_t newsize, size_t alignment, size_t offset) pa_attr_noexcept pa_attr_alloc_size(3);
pa_decl_nodiscard pa_decl_export void* pa_heap_recalloc_aligned(pa_heap_t* heap, void* p, size_t newcount, size_t size, size_t alignment) pa_attr_noexcept pa_attr_alloc_size2(3,4) pa_attr_alloc_align(5);
pa_decl_nodiscard pa_decl_export void* pa_heap_recalloc_aligned_at(pa_heap_t* heap, void* p, size_t newcount, size_t size, size_t alignment, size_t offset) pa_attr_noexcept pa_attr_alloc_size2(3,4);


// ------------------------------------------------------
// Analysis
// ------------------------------------------------------

pa_decl_export bool pa_heap_contains_block(pa_heap_t* heap, const void* p);
pa_decl_export bool pa_heap_check_owned(pa_heap_t* heap, const void* p);
pa_decl_export bool pa_check_owned(const void* p);

// An area of heap space contains blocks of a single size.
typedef struct pa_heap_area_s {
  void*  blocks;      // start of the area containing heap blocks
  size_t reserved;    // bytes reserved for this area (virtual)
  size_t committed;   // current available bytes for this area
  size_t used;        // number of allocated blocks
  size_t block_size;  // size in bytes of each block
  size_t full_block_size; // size in bytes of a full block including padding and metadata.
  int    heap_tag;    // heap tag associated with this area
} pa_heap_area_t;

typedef bool (pa_cdecl pa_block_visit_fun)(const pa_heap_t* heap, const pa_heap_area_t* area, void* block, size_t block_size, void* arg);

pa_decl_export bool pa_heap_visit_blocks(const pa_heap_t* heap, bool visit_blocks, pa_block_visit_fun* visitor, void* arg);

// Experimental
pa_decl_nodiscard pa_decl_export bool pa_is_in_heap_region(const void* p) pa_attr_noexcept;
pa_decl_nodiscard pa_decl_export bool pa_is_redirected(void) pa_attr_noexcept;

pa_decl_export int   pa_reserve_huge_os_pages_interleave(size_t pages, size_t numa_nodes, size_t timeout_msecs) pa_attr_noexcept;
pa_decl_export int   pa_reserve_huge_os_pages_at(size_t pages, int numa_node, size_t timeout_msecs) pa_attr_noexcept;

pa_decl_export int   pa_reserve_os_memory(size_t size, bool commit, bool allow_large) pa_attr_noexcept;
pa_decl_export bool  pa_manage_os_memory(void* start, size_t size, bool is_committed, bool is_large, bool is_zero, int numa_node) pa_attr_noexcept;

pa_decl_export void  pa_debug_show_arenas(void) pa_attr_noexcept;
pa_decl_export void  pa_arenas_print(void) pa_attr_noexcept;

// Experimental: heaps associated with specific memory arena's
typedef int pa_arena_id_t;
pa_decl_export void* pa_arena_area(pa_arena_id_t arena_id, size_t* size);
pa_decl_export int   pa_reserve_huge_os_pages_at_ex(size_t pages, int numa_node, size_t timeout_msecs, bool exclusive, pa_arena_id_t* arena_id) pa_attr_noexcept;
pa_decl_export int   pa_reserve_os_memory_ex(size_t size, bool commit, bool allow_large, bool exclusive, pa_arena_id_t* arena_id) pa_attr_noexcept;
pa_decl_export bool  pa_manage_os_memory_ex(void* start, size_t size, bool is_committed, bool is_large, bool is_zero, int numa_node, bool exclusive, pa_arena_id_t* arena_id) pa_attr_noexcept;

#if PA_MALLOC_VERSION >= 182
// Create a heap that only allocates in the specified arena
pa_decl_nodiscard pa_decl_export pa_heap_t* pa_heap_new_in_arena(pa_arena_id_t arena_id);
#endif


// Experimental: allow sub-processes whose memory areas stay separated (and no reclamation between them)
// Used for example for separate interpreters in one process.
typedef void* pa_subproc_id_t;
pa_decl_export pa_subproc_id_t pa_subproc_main(void);
pa_decl_export pa_subproc_id_t pa_subproc_new(void);
pa_decl_export void pa_subproc_delete(pa_subproc_id_t subproc);
pa_decl_export void pa_subproc_add_current_thread(pa_subproc_id_t subproc); // this should be called right after a thread is created (and no allocation has taken place yet)

// Experimental: visit abandoned heap areas (that are not owned by a specific heap)
pa_decl_export bool pa_abandoned_visit_blocks(pa_subproc_id_t subproc_id, int heap_tag, bool visit_blocks, pa_block_visit_fun* visitor, void* arg);

// Experimental: objects followed by a guard page.
// A sample rate of 0 disables guarded objects, while 1 uses a guard page for every object.
// A seed of 0 uses a random start point. Only objects within the size bound are eligable for guard pages.
pa_decl_export void pa_heap_guarded_set_sample_rate(pa_heap_t* heap, size_t sample_rate, size_t seed);
pa_decl_export void pa_heap_guarded_set_size_bound(pa_heap_t* heap, size_t min, size_t max);

// Experimental: communicate that the thread is part of a threadpool
pa_decl_export void pa_thread_set_in_threadpool(void) pa_attr_noexcept;

// Experimental: create a new heap with a specified heap tag. Set `allow_destroy` to false to allow the thread
// to reclaim abandoned memory (with a compatible heap_tag and arena_id) but in that case `pa_heap_destroy` will
// fall back to `pa_heap_delete`.
pa_decl_nodiscard pa_decl_export pa_heap_t* pa_heap_new_ex(int heap_tag, bool allow_destroy, pa_arena_id_t arena_id);

// deprecated
pa_decl_export int pa_reserve_huge_os_pages(size_t pages, double max_secs, size_t* pages_reserved) pa_attr_noexcept;
pa_decl_export void pa_collect_reduce(size_t target_thread_owned) pa_attr_noexcept;



// ------------------------------------------------------
// Convenience
// ------------------------------------------------------

#define pa_malloc_tp(tp)                ((tp*)pa_malloc(sizeof(tp)))
#define pa_zalloc_tp(tp)                ((tp*)pa_zalloc(sizeof(tp)))
#define pa_calloc_tp(tp,n)              ((tp*)pa_calloc(n,sizeof(tp)))
#define pa_mallocn_tp(tp,n)             ((tp*)pa_mallocn(n,sizeof(tp)))
#define pa_reallocn_tp(p,tp,n)          ((tp*)pa_reallocn(p,n,sizeof(tp)))
#define pa_recalloc_tp(p,tp,n)          ((tp*)pa_recalloc(p,n,sizeof(tp)))

#define pa_heap_malloc_tp(hp,tp)        ((tp*)pa_heap_malloc(hp,sizeof(tp)))
#define pa_heap_zalloc_tp(hp,tp)        ((tp*)pa_heap_zalloc(hp,sizeof(tp)))
#define pa_heap_calloc_tp(hp,tp,n)      ((tp*)pa_heap_calloc(hp,n,sizeof(tp)))
#define pa_heap_mallocn_tp(hp,tp,n)     ((tp*)pa_heap_mallocn(hp,n,sizeof(tp)))
#define pa_heap_reallocn_tp(hp,p,tp,n)  ((tp*)pa_heap_reallocn(hp,p,n,sizeof(tp)))
#define pa_heap_recalloc_tp(hp,p,tp,n)  ((tp*)pa_heap_recalloc(hp,p,n,sizeof(tp)))


// ------------------------------------------------------
// Options
// ------------------------------------------------------

typedef enum pa_option_e {
  // stable options
  pa_option_show_errors,                // print error messages
  pa_option_show_stats,                 // print statistics on termination
  pa_option_verbose,                    // print verbose messages
  // advanced options
  pa_option_eager_commit,               // eager commit segments? (after `eager_commit_delay` segments) (=1)
  pa_option_arena_eager_commit,         // eager commit arenas? Use 2 to enable just on overcommit systems (=2)
  pa_option_purge_decommits,            // should a memory purge decommit? (=1). Set to 0 to use memory reset on a purge (instead of decommit)
  pa_option_allow_large_os_pages,       // allow use of large (2 or 4 MiB) OS pages, implies eager commit.
  pa_option_reserve_huge_os_pages,      // reserve N huge OS pages (1GiB pages) at startup
  pa_option_reserve_huge_os_pages_at,   // reserve huge OS pages at a specific NUMA node
  pa_option_reserve_os_memory,          // reserve specified amount of OS memory in an arena at startup (internally, this value is in KiB; use `pa_option_get_size`)
  pa_option_deprecated_segment_cache,
  pa_option_deprecated_page_reset,
  pa_option_abandoned_page_purge,       // immediately purge delayed purges on thread termination
  pa_option_deprecated_segment_reset,
  pa_option_eager_commit_delay,         // the first N segments per thread are not eagerly committed (but per page in the segment on demand)
  pa_option_purge_delay,                // memory purging is delayed by N milli seconds; use 0 for immediate purging or -1 for no purging at all. (=10)
  pa_option_use_numa_nodes,             // 0 = use all available numa nodes, otherwise use at most N nodes.
  pa_option_disallow_os_alloc,          // 1 = do not use OS memory for allocation (but only programmatically reserved arenas)
  pa_option_os_tag,                     // tag used for OS logging (macOS only for now) (=100)
  pa_option_max_errors,                 // issue at most N error messages
  pa_option_max_warnings,               // issue at most N warning messages
  pa_option_max_segment_reclaim,        // max. percentage of the abandoned segments can be reclaimed per try (=10%)
  pa_option_destroy_on_exit,            // if set, release all memory on exit; sometimes used for dynamic unloading but can be unsafe
  pa_option_arena_reserve,              // initial memory size for arena reservation (= 1 GiB on 64-bit) (internally, this value is in KiB; use `pa_option_get_size`)
  pa_option_arena_purge_mult,           // multiplier for `purge_delay` for the purging delay for arenas (=10)
  pa_option_purge_extend_delay,
  pa_option_abandoned_reclaim_on_free,  // allow to reclaim an abandoned segment on a free (=1)
  pa_option_disallow_arena_alloc,       // 1 = do not use arena's for allocation (except if using specific arena id's)
  pa_option_retry_on_oom,               // retry on out-of-memory for N milli seconds (=400), set to 0 to disable retries. (only on windows)
  pa_option_visit_abandoned,            // allow visiting heap blocks from abandoned threads (=0)
  pa_option_guarded_min,                // only used when building with PA_GUARDED: minimal rounded object size for guarded objects (=0)
  pa_option_guarded_max,                // only used when building with PA_GUARDED: maximal rounded object size for guarded objects (=0)
  pa_option_guarded_precise,            // disregard minimal alignment requirement to always place guarded blocks exactly in front of a guard page (=0)
  pa_option_guarded_sample_rate,        // 1 out of N allocations in the min/max range will be guarded (=1000)
  pa_option_guarded_sample_seed,        // can be set to allow for a (more) deterministic re-execution when a guard page is triggered (=0)
  pa_option_target_segments_per_thread, // experimental (=0)
  pa_option_generic_collect,            // collect heaps every N (=10000) generic allocation calls
  pa_option_allow_thp,                  // allow transparent huge pages? (=1) (on Android =0 by default). Set to 0 to disable THP for the process.
  pa_option_adaptive_purge,             // adaptively adjust purge delay based on workload churn (=1)
  pa_option_purge_background,           // purge memory in the background? (0=disabled, 1=enabled)
  pa_option_zero_background,            // pre-zero memory in the background? (0=disabled, 1=enabled)
  _pa_option_last,
  // legacy option names
  pa_option_large_os_pages = pa_option_allow_large_os_pages,
  pa_option_eager_region_commit = pa_option_arena_eager_commit,
  pa_option_reset_decommits = pa_option_purge_decommits,
  pa_option_reset_delay = pa_option_purge_delay,
  pa_option_abandoned_page_reset = pa_option_abandoned_page_purge,
  pa_option_limit_os_alloc = pa_option_disallow_os_alloc
} pa_option_t;


pa_decl_nodiscard pa_decl_export bool pa_option_is_enabled(pa_option_t option);
pa_decl_export void pa_option_enable(pa_option_t option);
pa_decl_export void pa_option_disable(pa_option_t option);
pa_decl_export void pa_option_set_enabled(pa_option_t option, bool enable);
pa_decl_export void pa_option_set_enabled_default(pa_option_t option, bool enable);

pa_decl_nodiscard pa_decl_export long   pa_option_get(pa_option_t option);
pa_decl_nodiscard pa_decl_export long   pa_option_get_clamp(pa_option_t option, long min, long max);
pa_decl_nodiscard pa_decl_export size_t pa_option_get_size(pa_option_t option);
pa_decl_export void pa_option_set(pa_option_t option, long value);
pa_decl_export void pa_option_set_default(pa_option_t option, long value);


// -------------------------------------------------------------------------------------------------------
// "mi" prefixed implementations of various posix, Unix, Windows, and C++ allocation functions.
// (This can be convenient when providing overrides of these functions as done in `palloc-override.h`.)
// note: we use `pa_cfree` as "checked free" and it checks if the pointer is in our heap before free-ing.
// -------------------------------------------------------------------------------------------------------

pa_decl_export void  pa_cfree(void* p) pa_attr_noexcept;
pa_decl_export void* pa__expand(void* p, size_t newsize) pa_attr_noexcept;
pa_decl_nodiscard pa_decl_export size_t pa_malloc_size(const void* p)        pa_attr_noexcept;
pa_decl_nodiscard pa_decl_export size_t pa_malloc_good_size(size_t size)     pa_attr_noexcept;
pa_decl_nodiscard pa_decl_export size_t pa_malloc_usable_size(const void *p) pa_attr_noexcept;

pa_decl_export int pa_posix_memalign(void** p, size_t alignment, size_t size)   pa_attr_noexcept;
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_memalign(size_t alignment, size_t size) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(2) pa_attr_alloc_align(1);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_valloc(size_t size)  pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_pvalloc(size_t size) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_aligned_alloc(size_t alignment, size_t size) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(2) pa_attr_alloc_align(1);

pa_decl_nodiscard pa_decl_export void* pa_reallocarray(void* p, size_t count, size_t size) pa_attr_noexcept pa_attr_alloc_size2(2,3);
pa_decl_nodiscard pa_decl_export int   pa_reallocarr(void* p, size_t count, size_t size) pa_attr_noexcept;
pa_decl_nodiscard pa_decl_export void* pa_aligned_recalloc(void* p, size_t newcount, size_t size, size_t alignment) pa_attr_noexcept;
pa_decl_nodiscard pa_decl_export void* pa_aligned_offset_recalloc(void* p, size_t newcount, size_t size, size_t alignment, size_t offset) pa_attr_noexcept;

pa_decl_nodiscard pa_decl_export pa_decl_restrict unsigned short* pa_wcsdup(const unsigned short* s) pa_attr_noexcept pa_attr_malloc;
pa_decl_nodiscard pa_decl_export pa_decl_restrict unsigned char*  pa_mbsdup(const unsigned char* s)  pa_attr_noexcept pa_attr_malloc;
pa_decl_export int pa_dupenv_s(char** buf, size_t* size, const char* name)                      pa_attr_noexcept;
pa_decl_export int pa_wdupenv_s(unsigned short** buf, size_t* size, const unsigned short* name) pa_attr_noexcept;

pa_decl_export void pa_free_size(void* p, size_t size)                           pa_attr_noexcept;
pa_decl_export void pa_free_size_aligned(void* p, size_t size, size_t alignment) pa_attr_noexcept;
pa_decl_export void pa_free_aligned(void* p, size_t alignment)                   pa_attr_noexcept;

// The `pa_new` wrappers implement C++ semantics on out-of-memory instead of directly returning `NULL`.
// (and call `std::get_new_handler` and potentially raise a `std::bad_alloc` exception).
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_new(size_t size)                   pa_attr_malloc pa_attr_alloc_size(1);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_new_aligned(size_t size, size_t alignment) pa_attr_malloc pa_attr_alloc_size(1) pa_attr_alloc_align(2);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_new_nothrow(size_t size)           pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_new_aligned_nothrow(size_t size, size_t alignment) pa_attr_noexcept pa_attr_malloc pa_attr_alloc_size(1) pa_attr_alloc_align(2);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_new_n(size_t count, size_t size)   pa_attr_malloc pa_attr_alloc_size2(1, 2);
pa_decl_nodiscard pa_decl_export void* pa_new_realloc(void* p, size_t newsize)                pa_attr_alloc_size(2);
pa_decl_nodiscard pa_decl_export void* pa_new_reallocn(void* p, size_t newcount, size_t size) pa_attr_alloc_size2(2, 3);

pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_heap_alloc_new(pa_heap_t* heap, size_t size)                pa_attr_malloc pa_attr_alloc_size(2);
pa_decl_nodiscard pa_decl_export pa_decl_restrict void* pa_heap_alloc_new_n(pa_heap_t* heap, size_t count, size_t size) pa_attr_malloc pa_attr_alloc_size2(2, 3);

#ifdef __cplusplus
}
#endif

// ---------------------------------------------------------------------------------------------
// Implement the C++ std::allocator interface for use in STL containers.
// (note: see `palloc-new-delete.h` for overriding the new/delete operators globally)
// ---------------------------------------------------------------------------------------------
#ifdef __cplusplus

#include <cstddef>     // std::size_t
#include <cstdint>     // PTRDIFF_MAX
#if (__cplusplus >= 201103L) || (_MSC_VER > 1900)  // C++11
#include <type_traits> // std::true_type
#include <utility>     // std::forward
#endif

template<class T> struct _pa_stl_allocator_common {
  typedef T                 value_type;
  typedef std::size_t       size_type;
  typedef std::ptrdiff_t    difference_type;
  typedef value_type&       reference;
  typedef value_type const& const_reference;
  typedef value_type*       pointer;
  typedef value_type const* const_pointer;

  #if ((__cplusplus >= 201103L) || (_MSC_VER > 1900))  // C++11
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap            = std::true_type;
  template <class U, class ...Args> void construct(U* p, Args&& ...args) { ::new(p) U(std::forward<Args>(args)...); }
  template <class U> void destroy(U* p) pa_attr_noexcept { p->~U(); }
  #else
  void construct(pointer p, value_type const& val) { ::new(p) value_type(val); }
  void destroy(pointer p) { p->~value_type(); }
  #endif

  size_type     max_size() const pa_attr_noexcept { return (PTRDIFF_MAX/sizeof(value_type)); }
  pointer       address(reference x) const        { return &x; }
  const_pointer address(const_reference x) const  { return &x; }
};

template<class T> struct pa_stl_allocator : public _pa_stl_allocator_common<T> {
  using typename _pa_stl_allocator_common<T>::size_type;
  using typename _pa_stl_allocator_common<T>::value_type;
  using typename _pa_stl_allocator_common<T>::pointer;
  template <class U> struct rebind { typedef pa_stl_allocator<U> other; };

  pa_stl_allocator()                                             pa_attr_noexcept = default;
  pa_stl_allocator(const pa_stl_allocator&)                      pa_attr_noexcept = default;
  template<class U> pa_stl_allocator(const pa_stl_allocator<U>&) pa_attr_noexcept { }
  pa_stl_allocator  select_on_container_copy_construction() const { return *this; }
  void              deallocate(T* p, size_type) { pa_free(p); }

  #if (__cplusplus >= 201703L)  // C++17
  pa_decl_nodiscard T* allocate(size_type count) { return static_cast<T*>(pa_new_n(count, sizeof(T))); }
  pa_decl_nodiscard T* allocate(size_type count, const void*) { return allocate(count); }
  #else
  pa_decl_nodiscard pointer allocate(size_type count, const void* = 0) { return static_cast<pointer>(pa_new_n(count, sizeof(value_type))); }
  #endif

  #if ((__cplusplus >= 201103L) || (_MSC_VER > 1900))  // C++11
  using is_always_equal = std::true_type;
  #endif
};

template<class T1,class T2> bool operator==(const pa_stl_allocator<T1>& , const pa_stl_allocator<T2>& ) pa_attr_noexcept { return true; }
template<class T1,class T2> bool operator!=(const pa_stl_allocator<T1>& , const pa_stl_allocator<T2>& ) pa_attr_noexcept { return false; }


#if (__cplusplus >= 201103L) || (_MSC_VER >= 1900)  // C++11
#define PA_HAS_HEAP_STL_ALLOCATOR 1

#include <memory>      // std::shared_ptr

// Common base class for STL allocators in a specific heap
template<class T, bool _pa_destroy> struct _pa_heap_stl_allocator_common : public _pa_stl_allocator_common<T> {
  using typename _pa_stl_allocator_common<T>::size_type;
  using typename _pa_stl_allocator_common<T>::value_type;
  using typename _pa_stl_allocator_common<T>::pointer;

  _pa_heap_stl_allocator_common(pa_heap_t* hp) : heap(hp, [](pa_heap_t*) {}) {}    /* will not delete nor destroy the passed in heap */

  #if (__cplusplus >= 201703L)  // C++17
  pa_decl_nodiscard T* allocate(size_type count) { return static_cast<T*>(pa_heap_alloc_new_n(this->heap.get(), count, sizeof(T))); }
  pa_decl_nodiscard T* allocate(size_type count, const void*) { return allocate(count); }
  #else
  pa_decl_nodiscard pointer allocate(size_type count, const void* = 0) { return static_cast<pointer>(pa_heap_alloc_new_n(this->heap.get(), count, sizeof(value_type))); }
  #endif

  #if ((__cplusplus >= 201103L) || (_MSC_VER > 1900))  // C++11
  using is_always_equal = std::false_type;
  #endif

  void collect(bool force) { pa_heap_collect(this->heap.get(), force); }
  template<class U> bool is_equal(const _pa_heap_stl_allocator_common<U, _pa_destroy>& x) const { return (this->heap == x.heap); }

protected:
  std::shared_ptr<pa_heap_t> heap;
  template<class U, bool D> friend struct _pa_heap_stl_allocator_common;

  _pa_heap_stl_allocator_common() {
    pa_heap_t* hp = pa_heap_new();
    this->heap.reset(hp, (_pa_destroy ? &heap_destroy : &heap_delete));  /* calls heap_delete/destroy when the refcount drops to zero */
  }
  _pa_heap_stl_allocator_common(const _pa_heap_stl_allocator_common& x) pa_attr_noexcept : heap(x.heap) { }
  template<class U> _pa_heap_stl_allocator_common(const _pa_heap_stl_allocator_common<U, _pa_destroy>& x) pa_attr_noexcept : heap(x.heap) { }

private:
  static void heap_delete(pa_heap_t* hp)  { if (hp != NULL) { pa_heap_delete(hp); } }
  static void heap_destroy(pa_heap_t* hp) { if (hp != NULL) { pa_heap_destroy(hp); } }
};

// STL allocator allocation in a specific heap
template<class T> struct pa_heap_stl_allocator : public _pa_heap_stl_allocator_common<T, false> {
  using typename _pa_heap_stl_allocator_common<T, false>::size_type;
  pa_heap_stl_allocator() : _pa_heap_stl_allocator_common<T, false>() { } // creates fresh heap that is deleted when the destructor is called
  pa_heap_stl_allocator(pa_heap_t* hp) : _pa_heap_stl_allocator_common<T, false>(hp) { }  // no delete nor destroy on the passed in heap
  template<class U> pa_heap_stl_allocator(const pa_heap_stl_allocator<U>& x) pa_attr_noexcept : _pa_heap_stl_allocator_common<T, false>(x) { }

  pa_heap_stl_allocator select_on_container_copy_construction() const { return *this; }
  void deallocate(T* p, size_type) { pa_free(p); }
  template<class U> struct rebind { typedef pa_heap_stl_allocator<U> other; };
};

template<class T1, class T2> bool operator==(const pa_heap_stl_allocator<T1>& x, const pa_heap_stl_allocator<T2>& y) pa_attr_noexcept { return (x.is_equal(y)); }
template<class T1, class T2> bool operator!=(const pa_heap_stl_allocator<T1>& x, const pa_heap_stl_allocator<T2>& y) pa_attr_noexcept { return (!x.is_equal(y)); }


// STL allocator allocation in a specific heap, where `free` does nothing and
// the heap is destroyed in one go on destruction -- use with care!
template<class T> struct pa_heap_destroy_stl_allocator : public _pa_heap_stl_allocator_common<T, true> {
  using typename _pa_heap_stl_allocator_common<T, true>::size_type;
  pa_heap_destroy_stl_allocator() : _pa_heap_stl_allocator_common<T, true>() { } // creates fresh heap that is destroyed when the destructor is called
  pa_heap_destroy_stl_allocator(pa_heap_t* hp) : _pa_heap_stl_allocator_common<T, true>(hp) { }  // no delete nor destroy on the passed in heap
  template<class U> pa_heap_destroy_stl_allocator(const pa_heap_destroy_stl_allocator<U>& x) pa_attr_noexcept : _pa_heap_stl_allocator_common<T, true>(x) { }

  pa_heap_destroy_stl_allocator select_on_container_copy_construction() const { return *this; }
  void deallocate(T*, size_type) { /* do nothing as we destroy the heap on destruct. */ }
  template<class U> struct rebind { typedef pa_heap_destroy_stl_allocator<U> other; };
};

template<class T1, class T2> bool operator==(const pa_heap_destroy_stl_allocator<T1>& x, const pa_heap_destroy_stl_allocator<T2>& y) pa_attr_noexcept { return (x.is_equal(y)); }
template<class T1, class T2> bool operator!=(const pa_heap_destroy_stl_allocator<T1>& x, const pa_heap_destroy_stl_allocator<T2>& y) pa_attr_noexcept { return (!x.is_equal(y)); }

#endif // C++11

#endif // __cplusplus

#endif
