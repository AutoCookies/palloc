/* ----------------------------------------------------------------------------
Copyright (c) 2018-2023, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#pragma once
#ifndef PALLOC_INTERNAL_H
#define PALLOC_INTERNAL_H

// --------------------------------------------------------------------------
// This file contains the internal API's of palloc and various utility
// functions and macros.
// --------------------------------------------------------------------------

#include "types.h"
#include "track.h"


// --------------------------------------------------------------------------
// Compiler defines
// --------------------------------------------------------------------------

#if (PA_DEBUG>0)
#define pa_trace_message(...)  _pa_trace_message(__VA_ARGS__)
#else
#define pa_trace_message(...)
#endif

#define pa_decl_cache_align     pa_decl_align(64)

#if defined(_MSC_VER)
#pragma warning(disable:4127)   // suppress constant conditional warning (due to PA_SECURE paths)
#pragma warning(disable:26812)  // unscoped enum warning
#define pa_decl_noinline        __declspec(noinline)
#define pa_decl_thread          __declspec(thread)
#define pa_decl_align(a)        __declspec(align(a))
#define pa_decl_noreturn        __declspec(noreturn)
#define pa_decl_weak
#define pa_decl_hidden
#define pa_decl_cold
#elif (defined(__GNUC__) && (__GNUC__ >= 3)) || defined(__clang__) // includes clang and icc
#define pa_decl_noinline        __attribute__((noinline))
#define pa_decl_thread          __thread
#define pa_decl_align(a)        __attribute__((aligned(a)))
#define pa_decl_noreturn        __attribute__((noreturn))
#define pa_decl_weak            __attribute__((weak))
#define pa_decl_hidden          __attribute__((visibility("hidden")))
#if (__GNUC__ >= 4) || defined(__clang__)
#define pa_decl_cold            __attribute__((cold))
#else
#define pa_decl_cold
#endif
#elif __cplusplus >= 201103L    // c++11
#define pa_decl_noinline
#define pa_decl_thread          thread_local
#define pa_decl_align(a)        alignas(a)
#define pa_decl_noreturn        [[noreturn]]
#define pa_decl_weak
#define pa_decl_hidden
#define pa_decl_cold
#else
#define pa_decl_noinline
#define pa_decl_thread          __thread        // hope for the best :-)
#define pa_decl_align(a)
#define pa_decl_noreturn
#define pa_decl_weak
#define pa_decl_hidden
#define pa_decl_cold
#endif

#if defined(__GNUC__) || defined(__clang__)
#define pa_prefetch(x) __builtin_prefetch(x)
#else
#define pa_prefetch(x) ((void)0)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define pa_unlikely(x)     (__builtin_expect(!!(x),false))
#define pa_likely(x)       (__builtin_expect(!!(x),true))
#elif (defined(__cplusplus) && (__cplusplus >= 202002L)) || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#define pa_unlikely(x)     (x) [[unlikely]]
#define pa_likely(x)       (x) [[likely]]
#else
#define pa_unlikely(x)     (x)
#define pa_likely(x)       (x)
#endif

#ifndef __has_builtin
#define __has_builtin(x)    0
#endif

#if defined(__cplusplus)
#define pa_decl_externc     extern "C"
#else
#define pa_decl_externc
#endif

#if defined(__EMSCRIPTEN__) && !defined(__wasi__)
#define __wasi__
#endif


// --------------------------------------------------------------------------
// Internal functions
// --------------------------------------------------------------------------

// "libc.c"
#include    <stdarg.h>
int         _pa_vsnprintf(char* buf, size_t bufsize, const char* fmt, va_list args);
int         _pa_snprintf(char* buf, size_t buflen, const char* fmt, ...);
char        _pa_toupper(char c);
int         _pa_strnicmp(const char* s, const char* t, size_t n);
void        _pa_strlcpy(char* dest, const char* src, size_t dest_size);
void        _pa_strlcat(char* dest, const char* src, size_t dest_size);
size_t      _pa_strlen(const char* s);
size_t      _pa_strnlen(const char* s, size_t max_len);
bool        _pa_getenv(const char* name, char* result, size_t result_size);

// "options.c"
void        _pa_fputs(pa_output_fun* out, void* arg, const char* prefix, const char* message);
void        _pa_fprintf(pa_output_fun* out, void* arg, const char* fmt, ...);
void        _pa_message(const char* fmt, ...);
void        _pa_warning_message(const char* fmt, ...);
void        _pa_verbose_message(const char* fmt, ...);
void        _pa_trace_message(const char* fmt, ...);
void        _pa_options_init(void);
long        _pa_option_get_fast(pa_option_t option);
void        _pa_error_message(int err, const char* fmt, ...);

// random.c
void        _pa_random_init(pa_random_ctx_t* ctx);
void        _pa_random_init_weak(pa_random_ctx_t* ctx);
void        _pa_random_reinit_if_weak(pa_random_ctx_t * ctx);
void        _pa_random_split(pa_random_ctx_t* ctx, pa_random_ctx_t* new_ctx);
uintptr_t   _pa_random_next(pa_random_ctx_t* ctx);
uintptr_t   _pa_heap_random_next(pa_heap_t* heap);
uintptr_t   _pa_os_random_weak(uintptr_t extra_seed);
static inline uintptr_t _pa_random_shuffle(uintptr_t x);

// init.c
extern pa_decl_hidden pa_decl_cache_align pa_stats_t       _pa_stats_main;
extern pa_decl_hidden pa_decl_cache_align const pa_page_t  _pa_page_empty;
void        _pa_auto_process_init(void);
void pa_cdecl _pa_auto_process_done(void) pa_attr_noexcept;
bool        _pa_is_redirected(void);
bool        _pa_allocator_init(const char** message);
void        _pa_allocator_done(void);
bool        _pa_is_main_thread(void);
size_t      _pa_current_thread_count(void);
bool        _pa_preloading(void);           // true while the C runtime is not initialized yet
void        _pa_thread_done(pa_heap_t* heap);
void        _pa_thread_data_collect(void);
void        _pa_tld_init(pa_tld_t* tld, pa_heap_t* bheap);
pa_threadid_t _pa_thread_id(void) pa_attr_noexcept;
pa_heap_t*    _pa_heap_main_get(void);     // statically allocated main backing heap
pa_subproc_t* _pa_subproc_from_id(pa_subproc_id_t subproc_id);
void        _pa_heap_guarded_init(pa_heap_t* heap);

// os.c
void        _pa_os_init(void);                                            // called from process init
void*       _pa_os_alloc(size_t size, pa_memid_t* memid);
void*       _pa_os_zalloc(size_t size, pa_memid_t* memid);
void        _pa_os_free(void* p, size_t size, pa_memid_t memid);
void        _pa_os_free_ex(void* p, size_t size, bool still_committed, pa_memid_t memid);

size_t      _pa_os_page_size(void);
size_t      _pa_os_good_alloc_size(size_t size);
bool        _pa_os_has_overcommit(void);
bool        _pa_os_has_virtual_reserve(void);

bool        _pa_os_reset(void* addr, size_t size);
bool        _pa_os_decommit(void* addr, size_t size);
bool        _pa_os_unprotect(void* addr, size_t size);
bool        _pa_os_purge(void* p, size_t size);
bool        _pa_os_purge_ex(void* p, size_t size, bool allow_reset, size_t stat_size);
void        _pa_os_reuse(void* p, size_t size);
pa_decl_nodiscard bool _pa_os_commit(void* p, size_t size, bool* is_zero);
pa_decl_nodiscard bool _pa_os_commit_ex(void* addr, size_t size, bool* is_zero, size_t stat_size);
bool        _pa_os_protect(void* addr, size_t size);

void*       _pa_os_alloc_aligned(size_t size, size_t alignment, bool commit, bool allow_large, pa_memid_t* memid);
void*       _pa_os_alloc_aligned_at_offset(size_t size, size_t alignment, size_t align_offset, bool commit, bool allow_large, pa_memid_t* memid);

void*       _pa_os_get_aligned_hint(size_t try_alignment, size_t size);
bool        _pa_os_canuse_large_page(size_t size, size_t alignment);
size_t      _pa_os_large_page_size(void);
void*       _pa_os_alloc_huge_os_pages(size_t pages, int numa_node, pa_msecs_t max_secs, size_t* pages_reserved, size_t* psize, pa_memid_t* memid);

int         _pa_os_numa_node_count(void);
int         _pa_os_numa_node(void);

// arena.c
pa_arena_id_t _pa_arena_id_none(void);
void        _pa_arena_free(void* p, size_t size, size_t still_committed_size, pa_memid_t memid);
void*       _pa_arena_alloc(size_t size, bool commit, bool allow_large, pa_arena_id_t req_arena_id, pa_memid_t* memid);
void*       _pa_arena_alloc_aligned(size_t size, size_t alignment, size_t align_offset, bool commit, bool allow_large, pa_arena_id_t req_arena_id, pa_memid_t* memid);
bool        _pa_arena_memid_is_suitable(pa_memid_t memid, pa_arena_id_t request_arena_id);
bool        _pa_arena_contains(const void* p);
void        _pa_arenas_collect(bool force_purge);
void        _pa_arena_unsafe_destroy_all(void);

bool        _pa_arena_segment_clear_abandoned(pa_segment_t* segment);
void        _pa_arena_segment_mark_abandoned(pa_segment_t* segment);

void*       _pa_arena_meta_zalloc(size_t size, pa_memid_t* memid);
void        _pa_arena_meta_free(void* p, pa_memid_t memid, size_t size);

typedef struct pa_arena_field_cursor_s { // abstract struct
  size_t         os_list_count;           // max entries to visit in the OS abandoned list
  size_t         start;                   // start arena idx (may need to be wrapped)
  size_t         end;                     // end arena idx (exclusive, may need to be wrapped)
  size_t         bitmap_idx;              // current bit idx for an arena
  pa_subproc_t*  subproc;                 // only visit blocks in this sub-process
  bool           visit_all;               // ensure all abandoned blocks are seen (blocking)
  bool           hold_visit_lock;         // if the subproc->abandoned_os_visit_lock is held
} pa_arena_field_cursor_t;
void          _pa_arena_field_cursor_init(pa_heap_t* heap, pa_subproc_t* subproc, bool visit_all, pa_arena_field_cursor_t* current);
pa_segment_t* _pa_arena_segment_clear_abandoned_next(pa_arena_field_cursor_t* previous);
void          _pa_arena_field_cursor_done(pa_arena_field_cursor_t* current);

// "segment-map.c"
void        _pa_segment_map_allocated_at(const pa_segment_t* segment);
void        _pa_segment_map_freed_at(const pa_segment_t* segment);
void        _pa_segment_map_unsafe_destroy(void);

// "segment.c"
pa_page_t* _pa_segment_page_alloc(pa_heap_t* heap, size_t block_size, size_t page_alignment, pa_segments_tld_t* tld);
void       _pa_segment_page_free(pa_page_t* page, bool force, pa_segments_tld_t* tld);
void       _pa_segment_page_abandon(pa_page_t* page, pa_segments_tld_t* tld);
bool       _pa_segment_try_reclaim_abandoned( pa_heap_t* heap, bool try_all, pa_segments_tld_t* tld);
void       _pa_segment_collect(pa_segment_t* segment, bool force);

#if PA_HUGE_PAGE_ABANDON
void        _pa_segment_huge_page_free(pa_segment_t* segment, pa_page_t* page, pa_block_t* block);
#else
void        _pa_segment_huge_page_reset(pa_segment_t* segment, pa_page_t* page, pa_block_t* block);
#endif

uint8_t*   _pa_segment_page_start(const pa_segment_t* segment, const pa_page_t* page, size_t* page_size); // page start for any page
void       _pa_abandoned_reclaim_all(pa_heap_t* heap, pa_segments_tld_t* tld);
void       _pa_abandoned_collect(pa_heap_t* heap, bool force, pa_segments_tld_t* tld);
bool       _pa_segment_attempt_reclaim(pa_heap_t* heap, pa_segment_t* segment);
bool       _pa_segment_visit_blocks(pa_segment_t* segment, int heap_tag, bool visit_blocks, pa_block_visit_fun* visitor, void* arg);

// "page.c"
void*       _pa_malloc_generic(pa_heap_t* heap, size_t size, bool zero, size_t huge_alignment, size_t* usable)  pa_attr_noexcept pa_attr_malloc;

void        _pa_page_retire(pa_page_t* page) pa_attr_noexcept;                  // free the page if there are no other pages with many free blocks
void        _pa_page_unfull(pa_page_t* page);
void        _pa_page_free(pa_page_t* page, pa_page_queue_t* pq, bool force);   // free the page
void        _pa_page_abandon(pa_page_t* page, pa_page_queue_t* pq);            // abandon the page, to be picked up by another thread...
void        _pa_page_force_abandon(pa_page_t* page);

void        _pa_heap_delayed_free_all(pa_heap_t* heap);
bool        _pa_heap_delayed_free_partial(pa_heap_t* heap);
void        _pa_heap_collect_retired(pa_heap_t* heap, bool force);

void        _pa_page_use_delayed_free(pa_page_t* page, pa_delayed_t delay, bool override_never);
bool        _pa_page_try_use_delayed_free(pa_page_t* page, pa_delayed_t delay, bool override_never);
size_t      _pa_page_queue_append(pa_heap_t* heap, pa_page_queue_t* pq, pa_page_queue_t* append);
void        _pa_deferred_free(pa_heap_t* heap, bool force);

void        _pa_page_free_collect(pa_page_t* page,bool force);
void        _pa_page_reclaim(pa_heap_t* heap, pa_page_t* page);   // callback from segments

size_t      _pa_page_stats_bin(const pa_page_t* page); // for stats
size_t      _pa_bin_size(size_t bin);                  // for stats
size_t      _pa_bin(size_t size);                      // for stats

// "heap.c"
void        _pa_heap_init(pa_heap_t* heap, pa_tld_t* tld, pa_arena_id_t arena_id, bool noreclaim, uint8_t tag);
void        _pa_heap_destroy_pages(pa_heap_t* heap);
void        _pa_heap_collect_abandon(pa_heap_t* heap);
void        _pa_heap_set_default_direct(pa_heap_t* heap);
bool        _pa_heap_memid_is_suitable(pa_heap_t* heap, pa_memid_t memid);
void        _pa_heap_unsafe_destroy_all(pa_heap_t* heap);
pa_heap_t*  _pa_heap_by_tag(pa_heap_t* heap, uint8_t tag);
void        _pa_heap_area_init(pa_heap_area_t* area, pa_page_t* page);
bool        _pa_heap_area_visit_blocks(const pa_heap_area_t* area, pa_page_t* page, pa_block_visit_fun* visitor, void* arg);

// "stats.c"
void        _pa_stats_done(pa_stats_t* stats);
void        _pa_stats_merge_thread(pa_tld_t* tld);
pa_msecs_t  _pa_clock_now(void);
pa_msecs_t  _pa_clock_end(pa_msecs_t start);
pa_msecs_t  _pa_clock_start(void);

// "alloc.c"
void*       _pa_page_malloc_zero(pa_heap_t* heap, pa_page_t* page, size_t size, bool zero, size_t* usable) pa_attr_noexcept;  // called from `_pa_malloc_generic`
void*       _pa_page_malloc(pa_heap_t* heap, pa_page_t* page, size_t size) pa_attr_noexcept;                  // called from `_pa_heap_malloc_aligned`
void*       _pa_page_malloc_zeroed(pa_heap_t* heap, pa_page_t* page, size_t size) pa_attr_noexcept;           // called from `_pa_heap_malloc_aligned`
void*       _pa_heap_malloc_zero(pa_heap_t* heap, size_t size, bool zero) pa_attr_noexcept;
void*       _pa_heap_malloc_zero_ex(pa_heap_t* heap, size_t size, bool zero, size_t huge_alignment, size_t* usable) pa_attr_noexcept;     // called from `_pa_heap_malloc_aligned`
void*       _pa_heap_realloc_zero(pa_heap_t* heap, void* p, size_t newsize, bool zero, size_t* usable_pre, size_t* usable_post) pa_attr_noexcept;
pa_block_t* _pa_page_ptr_unalign(const pa_page_t* page, const void* p);
bool        _pa_free_delayed_block(pa_block_t* block);
void        _pa_free_generic(pa_segment_t* segment, pa_page_t* page, bool is_local, void* p) pa_attr_noexcept;  // for runtime integration
void        _pa_padding_shrink(const pa_page_t* page, const pa_block_t* block, const size_t min_size);

#if PA_DEBUG>1
bool        _pa_page_is_valid(pa_page_t* page);
#endif


/* -----------------------------------------------------------
  Error codes passed to `_pa_fatal_error`
  All are recoverable but EFAULT is a serious error and aborts by default in secure mode.
  For portability define undefined error codes using common Unix codes:
  <https://www-numi.fnal.gov/offline_software/srt_public_context/WebDocs/Errors/unix_system_errors.html>
----------------------------------------------------------- */
#include <errno.h>
#ifndef EAGAIN         // double free
#define EAGAIN (11)
#endif
#ifndef ENOMEM         // out of memory
#define ENOMEM (12)
#endif
#ifndef EFAULT         // corrupted free-list or meta-data
#define EFAULT (14)
#endif
#ifndef EINVAL         // trying to free an invalid pointer
#define EINVAL (22)
#endif
#ifndef EOVERFLOW      // count*size overflow
#define EOVERFLOW (75)
#endif


// ------------------------------------------------------
// Assertions
// ------------------------------------------------------

#if (PA_DEBUG)
// use our own assertion to print without memory allocation
pa_decl_noreturn pa_decl_cold void _pa_assert_fail(const char* assertion, const char* fname, unsigned int line, const char* func) pa_attr_noexcept;
#define pa_assert(expr)     ((expr) ? (void)0 : _pa_assert_fail(#expr,__FILE__,__LINE__,__func__))
#else
#define pa_assert(x)
#endif

#if (PA_DEBUG>1)
#define pa_assert_internal    pa_assert
#else
#define pa_assert_internal(x)
#endif

#if (PA_DEBUG>2)
#define pa_assert_expensive   pa_assert
#else
#define pa_assert_expensive(x)
#endif



/* -----------------------------------------------------------
  Inlined definitions
----------------------------------------------------------- */
#define PA_UNUSED(x)     (void)(x)
#if (PA_DEBUG>0)
#define PA_UNUSED_RELEASE(x)
#else
#define PA_UNUSED_RELEASE(x)  PA_UNUSED(x)
#endif

#define PA_INIT4(x)   x(),x(),x(),x()
#define PA_INIT8(x)   PA_INIT4(x),PA_INIT4(x)
#define PA_INIT16(x)  PA_INIT8(x),PA_INIT8(x)
#define PA_INIT32(x)  PA_INIT16(x),PA_INIT16(x)
#define PA_INIT64(x)  PA_INIT32(x),PA_INIT32(x)
#define PA_INIT128(x) PA_INIT64(x),PA_INIT64(x)
#define PA_INIT256(x) PA_INIT128(x),PA_INIT128(x)
#define PA_INIT74(x)  PA_INIT64(x),PA_INIT8(x),x(),x()

#include <string.h>
// initialize a local variable to zero; use memset as compilers optimize constant sized memset's
#define _pa_memzero_var(x)  memset(&x,0,sizeof(x))

// Is `x` a power of two? (0 is considered a power of two)
static inline bool _pa_is_power_of_two(uintptr_t x) {
  return ((x & (x - 1)) == 0);
}

// Is a pointer aligned?
static inline bool _pa_is_aligned(void* p, size_t alignment) {
  pa_assert_internal(alignment != 0);
  return (((uintptr_t)p % alignment) == 0);
}

// Align upwards
static inline uintptr_t _pa_align_up(uintptr_t sz, size_t alignment) {
  pa_assert_internal(alignment != 0);
  uintptr_t mask = alignment - 1;
  if ((alignment & mask) == 0) {  // power of two?
    return ((sz + mask) & ~mask);
  }
  else {
    return (((sz + mask)/alignment)*alignment);
  }
}

// Align downwards
static inline uintptr_t _pa_align_down(uintptr_t sz, size_t alignment) {
  pa_assert_internal(alignment != 0);
  uintptr_t mask = alignment - 1;
  if ((alignment & mask) == 0) { // power of two?
    return (sz & ~mask);
  }
  else {
    return ((sz / alignment) * alignment);
  }
}

// Align a pointer upwards
static inline void* pa_align_up_ptr(void* p, size_t alignment) {
  return (void*)_pa_align_up((uintptr_t)p, alignment);
}

// Align a pointer downwards
static inline void* pa_align_down_ptr(void* p, size_t alignment) {
  return (void*)_pa_align_down((uintptr_t)p, alignment);
}


// Divide upwards: `s <= _pa_divide_up(s,d)*d < s+d`.
static inline uintptr_t _pa_divide_up(uintptr_t size, size_t divider) {
  pa_assert_internal(divider != 0);
  return (divider == 0 ? size : ((size + divider - 1) / divider));
}


// clamp an integer
static inline size_t _pa_clamp(size_t sz, size_t min, size_t max) {
  if (sz < min) return min;
  else if (sz > max) return max;
  else return sz;
}

// Is memory zero initialized?
static inline bool pa_mem_is_zero(const void* p, size_t size) {
  for (size_t i = 0; i < size; i++) {
    if (((uint8_t*)p)[i] != 0) return false;
  }
  return true;
}


// Align a byte size to a size in _machine words_,
// i.e. byte size == `wsize*sizeof(void*)`.
static inline size_t _pa_wsize_from_size(size_t size) {
  pa_assert_internal(size <= SIZE_MAX - sizeof(uintptr_t));
  return (size + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);
}

// Overflow detecting multiply
#if __has_builtin(__builtin_umul_overflow) || (defined(__GNUC__) && (__GNUC__ >= 5))
#include <limits.h>      // UINT_MAX, ULONG_MAX
#if defined(_CLOCK_T)    // for Illumos
#undef _CLOCK_T
#endif
static inline bool pa_mul_overflow(size_t count, size_t size, size_t* total) {
  #if (SIZE_MAX == ULONG_MAX)
    return __builtin_umull_overflow(count, size, (unsigned long *)total);
  #elif (SIZE_MAX == UINT_MAX)
    return __builtin_umul_overflow(count, size, (unsigned int *)total);
  #else
    return __builtin_umulll_overflow(count, size, (unsigned long long *)total);
  #endif
}
#else /* __builtin_umul_overflow is unavailable */
static inline bool pa_mul_overflow(size_t count, size_t size, size_t* total) {
  #define PA_MUL_COULD_OVERFLOW ((size_t)1 << (4*sizeof(size_t)))  // sqrt(SIZE_MAX)
  *total = count * size;
  // note: gcc/clang optimize this to directly check the overflow flag
  return ((size >= PA_MUL_COULD_OVERFLOW || count >= PA_MUL_COULD_OVERFLOW) && size > 0 && (SIZE_MAX / size) < count);
}
#endif

// Safe multiply `count*size` into `total`; return `true` on overflow.
static inline bool pa_count_size_overflow(size_t count, size_t size, size_t* total) {
  if (count==1) {  // quick check for the case where count is one (common for C++ allocators)
    *total = size;
    return false;
  }
  else if pa_unlikely(pa_mul_overflow(count, size, total)) {
    #if PA_DEBUG > 0
    _pa_error_message(EOVERFLOW, "allocation request is too large (%zu * %zu bytes)\n", count, size);
    #endif
    *total = SIZE_MAX;
    return true;
  }
  else return false;
}


/*----------------------------------------------------------------------------------------
  Heap functions
------------------------------------------------------------------------------------------- */

extern pa_decl_hidden const pa_heap_t _pa_heap_empty;  // read-only empty heap, initial value of the thread local default heap

static inline bool pa_heap_is_backing(const pa_heap_t* heap) {
  return (heap->tld->heap_backing == heap);
}

static inline bool pa_heap_is_initialized(pa_heap_t* heap) {
  pa_assert_internal(heap != NULL);
  return (heap != NULL && heap != &_pa_heap_empty);
}

static inline uintptr_t _pa_ptr_cookie(const void* p) {
  extern pa_decl_hidden pa_heap_t _pa_heap_main;
  pa_assert_internal(_pa_heap_main.cookie != 0);
  return ((uintptr_t)p ^ _pa_heap_main.cookie);
}

/* -----------------------------------------------------------
  Pages
----------------------------------------------------------- */

static inline pa_page_t* _pa_heap_get_free_small_page(pa_heap_t* heap, size_t size) {
  pa_assert_internal(size <= (PA_SMALL_SIZE_MAX + PA_PADDING_SIZE));
  const size_t idx = _pa_wsize_from_size(size);
  pa_assert_internal(idx < PA_PAGES_DIRECT);
  return heap->pages_free_direct[idx];
}

// Segment that contains the pointer
// Large aligned blocks may be aligned at N*PA_SEGMENT_SIZE (inside a huge segment > PA_SEGMENT_SIZE),
// and we need align "down" to the segment info which is `PA_SEGMENT_SIZE` bytes before it;
// therefore we align one byte before `p`.
// We check for NULL afterwards on 64-bit systems to improve codegen for `pa_free`.
static inline pa_segment_t* _pa_ptr_segment(const void* p) {
  pa_segment_t* const segment = (pa_segment_t*)(((uintptr_t)p - 1) & ~PA_SEGMENT_MASK);
  #if PA_INTPTR_SIZE <= 4
  return (p==NULL ? NULL : segment);
  #else
  return ((intptr_t)segment <= 0 ? NULL : segment);
  #endif
}

static inline pa_page_t* pa_slice_to_page(pa_slice_t* s) {
  pa_assert_internal(s->slice_offset== 0 && s->slice_count > 0);
  return (pa_page_t*)(s);
}

static inline pa_slice_t* pa_page_to_slice(pa_page_t* p) {
  pa_assert_internal(p->slice_offset== 0 && p->slice_count > 0);
  return (pa_slice_t*)(p);
}

// Segment belonging to a page
static inline pa_segment_t* _pa_page_segment(const pa_page_t* page) {
  pa_assert_internal(page!=NULL);
  pa_segment_t* segment = _pa_ptr_segment(page);
  pa_assert_internal(segment == NULL || ((pa_slice_t*)page >= segment->slices && (pa_slice_t*)page < segment->slices + segment->slice_entries));
  return segment;
}

static inline pa_slice_t* pa_slice_first(const pa_slice_t* slice) {
  pa_slice_t* start = (pa_slice_t*)((uint8_t*)slice - slice->slice_offset);
  pa_assert_internal(start >= _pa_ptr_segment(slice)->slices);
  pa_assert_internal(start->slice_offset == 0);
  pa_assert_internal(start + start->slice_count > slice);
  return start;
}

// Get the page containing the pointer (performance critical as it is called in pa_free)
static inline pa_page_t* _pa_segment_page_of(const pa_segment_t* segment, const void* p) {
  pa_assert_internal(p > (void*)segment);
  ptrdiff_t diff = (uint8_t*)p - (uint8_t*)segment;
  pa_assert_internal(diff > 0 && diff <= (ptrdiff_t)PA_SEGMENT_SIZE);
  size_t idx = (size_t)diff >> PA_SEGMENT_SLICE_SHIFT;
  pa_assert_internal(idx <= segment->slice_entries);
  pa_slice_t* slice0 = (pa_slice_t*)&segment->slices[idx];
  pa_slice_t* slice = pa_slice_first(slice0);  // adjust to the block that holds the page data
  pa_assert_internal(slice->slice_offset == 0);
  pa_assert_internal(slice >= segment->slices && slice < segment->slices + segment->slice_entries);
  return pa_slice_to_page(slice);
}

// Quick page start for initialized pages
static inline uint8_t* pa_page_start(const pa_page_t* page) {
  pa_assert_internal(page->page_start != NULL);
  pa_assert_expensive(_pa_segment_page_start(_pa_page_segment(page),page,NULL) == page->page_start);
  return page->page_start;
}

// Get the page containing the pointer
static inline pa_page_t* _pa_ptr_page(void* p) {
  pa_assert_internal(p!=NULL);
  return _pa_segment_page_of(_pa_ptr_segment(p), p);
}

// Get the block size of a page (special case for huge objects)
static inline size_t pa_page_block_size(const pa_page_t* page) {
  pa_assert_internal(page->block_size > 0);
  return page->block_size;
}

static inline bool pa_page_is_huge(const pa_page_t* page) {
  pa_assert_internal((page->is_huge && _pa_page_segment(page)->kind == PA_SEGMENT_HUGE) ||
                     (!page->is_huge && _pa_page_segment(page)->kind != PA_SEGMENT_HUGE));
  return page->is_huge;
}

// Get the usable block size of a page without fixed padding.
// This may still include internal padding due to alignment and rounding up size classes.
static inline size_t pa_page_usable_block_size(const pa_page_t* page) {
  return pa_page_block_size(page) - PA_PADDING_SIZE;
}

// size of a segment
static inline size_t pa_segment_size(pa_segment_t* segment) {
  return segment->segment_slices * PA_SEGMENT_SLICE_SIZE;
}

static inline uint8_t* pa_segment_end(pa_segment_t* segment) {
  return (uint8_t*)segment + pa_segment_size(segment);
}

// Thread free access
static inline pa_block_t* pa_page_thread_free(const pa_page_t* page) {
  return (pa_block_t*)(pa_atomic_load_relaxed(&((pa_page_t*)page)->xthread_free) & ~3);
}

static inline pa_delayed_t pa_page_thread_free_flag(const pa_page_t* page) {
  return (pa_delayed_t)(pa_atomic_load_relaxed(&((pa_page_t*)page)->xthread_free) & 3);
}

// Heap access
static inline pa_heap_t* pa_page_heap(const pa_page_t* page) {
  return (pa_heap_t*)(pa_atomic_load_relaxed(&((pa_page_t*)page)->xheap));
}

static inline void pa_page_set_heap(pa_page_t* page, pa_heap_t* heap) {
  pa_assert_internal(pa_page_thread_free_flag(page) != PA_DELAYED_FREEING);
  pa_atomic_store_release(&page->xheap,(uintptr_t)heap);
  if (heap != NULL) { page->heap_tag = heap->tag; }
}

// Thread free flag helpers
static inline pa_block_t* pa_tf_block(pa_thread_free_t tf) {
  return (pa_block_t*)(tf & ~0x03);
}
static inline pa_delayed_t pa_tf_delayed(pa_thread_free_t tf) {
  return (pa_delayed_t)(tf & 0x03);
}
static inline pa_thread_free_t pa_tf_make(pa_block_t* block, pa_delayed_t delayed) {
  return (pa_thread_free_t)((uintptr_t)block | (uintptr_t)delayed);
}
static inline pa_thread_free_t pa_tf_set_delayed(pa_thread_free_t tf, pa_delayed_t delayed) {
  return pa_tf_make(pa_tf_block(tf),delayed);
}
static inline pa_thread_free_t pa_tf_set_block(pa_thread_free_t tf, pa_block_t* block) {
  return pa_tf_make(block, pa_tf_delayed(tf));
}

// are all blocks in a page freed?
// note: needs up-to-date used count, (as the `xthread_free` list may not be empty). see `_pa_page_collect_free`.
static inline bool pa_page_all_free(const pa_page_t* page) {
  pa_assert_internal(page != NULL);
  return (page->used == 0);
}

// are there any available blocks?
static inline bool pa_page_has_any_available(const pa_page_t* page) {
  pa_assert_internal(page != NULL && page->reserved > 0);
  return (page->used < page->reserved || (pa_page_thread_free(page) != NULL));
}

// are there immediately available blocks, i.e. blocks available on the free list.
static inline bool pa_page_immediate_available(const pa_page_t* page) {
  pa_assert_internal(page != NULL);
  return (page->free != NULL);
}

// is more than 7/8th of a page in use?
static inline bool pa_page_is_mostly_used(const pa_page_t* page) {
  if (page==NULL) return true;
  uint16_t frac = page->reserved / 8U;
  return (page->reserved - page->used <= frac);
}

static inline pa_page_queue_t* pa_page_queue(const pa_heap_t* heap, size_t size) {
  return &((pa_heap_t*)heap)->pages[_pa_bin(size)];
}



//-----------------------------------------------------------
// Page flags
//-----------------------------------------------------------
static inline bool pa_page_is_in_full(const pa_page_t* page) {
  return page->flags.x.in_full;
}

static inline void pa_page_set_in_full(pa_page_t* page, bool in_full) {
  page->flags.x.in_full = in_full;
}

static inline bool pa_page_has_aligned(const pa_page_t* page) {
  return page->flags.x.has_aligned;
}

static inline void pa_page_set_has_aligned(pa_page_t* page, bool has_aligned) {
  page->flags.x.has_aligned = has_aligned;
}

/* -------------------------------------------------------------------
  Guarded objects
------------------------------------------------------------------- */
#if PA_GUARDED
static inline bool pa_block_ptr_is_guarded(const pa_block_t* block, const void* p) {
  const ptrdiff_t offset = (uint8_t*)p - (uint8_t*)block;
  return (offset >= (ptrdiff_t)(sizeof(pa_block_t)) && block->next == PA_BLOCK_TAG_GUARDED);
}

static inline bool pa_heap_malloc_use_guarded(pa_heap_t* heap, size_t size) {
  // this code is written to result in fast assembly as it is on the hot path for allocation
  const size_t count = heap->guarded_sample_count - 1;  // if the rate was 0, this will underflow and count for a long time..
  if pa_likely(count != 0) {
    // no sample
    heap->guarded_sample_count = count;
    return false;
  }
  else if (size >= heap->guarded_size_min && size <= heap->guarded_size_max) {
    // use guarded allocation
    heap->guarded_sample_count = heap->guarded_sample_rate;  // reset
    return (heap->guarded_sample_rate != 0);
  }
  else {
    // failed size criteria, rewind count (but don't write to an empty heap)
    if (heap->guarded_sample_rate != 0) { heap->guarded_sample_count = 1; }
    return false;
  }
}

pa_decl_restrict void* _pa_heap_malloc_guarded(pa_heap_t* heap, size_t size, bool zero) pa_attr_noexcept;

#endif


/* -------------------------------------------------------------------
Encoding/Decoding the free list next pointers

This is to protect against buffer overflow exploits where the
free list is mutated. Many hardened allocators xor the next pointer `p`
with a secret key `k1`, as `p^k1`. This prevents overwriting with known
values but might be still too weak: if the attacker can guess
the pointer `p` this  can reveal `k1` (since `p^k1^p == k1`).
Moreover, if multiple blocks can be read as well, the attacker can
xor both as `(p1^k1) ^ (p2^k1) == p1^p2` which may reveal a lot
about the pointers (and subsequently `k1`).

Instead palloc uses an extra key `k2` and encodes as `((p^k2)<<<k1)+k1`.
Since these operations are not associative, the above approaches do not
work so well any more even if the `p` can be guesstimated. For example,
for the read case we can subtract two entries to discard the `+k1` term,
but that leads to `((p1^k2)<<<k1) - ((p2^k2)<<<k1)` at best.
We include the left-rotation since xor and addition are otherwise linear
in the lowest bit. Finally, both keys are unique per page which reduces
the re-use of keys by a large factor.

We also pass a separate `null` value to be used as `NULL` or otherwise
`(k2<<<k1)+k1` would appear (too) often as a sentinel value.
------------------------------------------------------------------- */

static inline bool pa_is_in_same_segment(const void* p, const void* q) {
  return (_pa_ptr_segment(p) == _pa_ptr_segment(q));
}

static inline bool pa_is_in_same_page(const void* p, const void* q) {
  pa_segment_t* segment = _pa_ptr_segment(p);
  if (_pa_ptr_segment(q) != segment) return false;
  // assume q may be invalid // return (_pa_segment_page_of(segment, p) == _pa_segment_page_of(segment, q));
  pa_page_t* page = _pa_segment_page_of(segment, p);
  size_t psize;
  uint8_t* start = _pa_segment_page_start(segment, page, &psize);
  return (start <= (uint8_t*)q && (uint8_t*)q < start + psize);
}

static inline uintptr_t pa_rotl(uintptr_t x, uintptr_t shift) {
  shift %= PA_INTPTR_BITS;
  return (shift==0 ? x : ((x << shift) | (x >> (PA_INTPTR_BITS - shift))));
}
static inline uintptr_t pa_rotr(uintptr_t x, uintptr_t shift) {
  shift %= PA_INTPTR_BITS;
  return (shift==0 ? x : ((x >> shift) | (x << (PA_INTPTR_BITS - shift))));
}

static inline void* pa_ptr_decode(const void* null, const pa_encoded_t x, const uintptr_t* keys) {
  void* p = (void*)(pa_rotr(x - keys[0], keys[0]) ^ keys[1]);
  return (p==null ? NULL : p);
}

static inline pa_encoded_t pa_ptr_encode(const void* null, const void* p, const uintptr_t* keys) {
  uintptr_t x = (uintptr_t)(p==NULL ? null : p);
  return pa_rotl(x ^ keys[1], keys[0]) + keys[0];
}

static inline uint32_t pa_ptr_encode_canary(const void* null, const void* p, const uintptr_t* keys) {
  const uint32_t x = (uint32_t)(pa_ptr_encode(null,p,keys));
  // make the lowest byte 0 to prevent spurious read overflows which could be a security issue (issue #951)
  #ifdef PA_BIG_ENDIAN
  return (x & 0x00FFFFFF);
  #else
  return (x & 0xFFFFFF00);
  #endif
}

static inline pa_block_t* pa_block_nextx( const void* null, const pa_block_t* block, const uintptr_t* keys ) {
  pa_track_mem_defined(block,sizeof(pa_block_t));
  pa_block_t* next;
  #ifdef PA_ENCODE_FREELIST
  next = (pa_block_t*)pa_ptr_decode(null, block->next, keys);
  #else
  PA_UNUSED(keys); PA_UNUSED(null);
  next = (pa_block_t*)block->next;
  #endif
  pa_track_mem_noaccess(block,sizeof(pa_block_t));
  return next;
}

static inline void pa_block_set_nextx(const void* null, pa_block_t* block, const pa_block_t* next, const uintptr_t* keys) {
  pa_track_mem_undefined(block,sizeof(pa_block_t));
  #ifdef PA_ENCODE_FREELIST
  block->next = pa_ptr_encode(null, next, keys);
  #else
  PA_UNUSED(keys); PA_UNUSED(null);
  block->next = (pa_encoded_t)next;
  #endif
  pa_track_mem_noaccess(block,sizeof(pa_block_t));
}

static inline pa_block_t* pa_block_next(const pa_page_t* page, const pa_block_t* block) {
  #ifdef PA_ENCODE_FREELIST
  pa_block_t* next = pa_block_nextx(page,block,page->keys);
  // check for free list corruption: is `next` at least in the same page?
  // TODO: check if `next` is `page->block_size` aligned?
  if pa_unlikely(next!=NULL && !pa_is_in_same_page(block, next)) {
    _pa_error_message(EFAULT, "corrupted free list entry of size %zub at %p: value 0x%zx\n", pa_page_block_size(page), block, (uintptr_t)next);
    next = NULL;
  }
  return next;
  #else
  PA_UNUSED(page);
  return pa_block_nextx(page,block,NULL);
  #endif
}

static inline void pa_block_set_next(const pa_page_t* page, pa_block_t* block, const pa_block_t* next) {
  #ifdef PA_ENCODE_FREELIST
  pa_block_set_nextx(page,block,next, page->keys);
  #else
  PA_UNUSED(page);
  pa_block_set_nextx(page,block,next,NULL);
  #endif
}


// -------------------------------------------------------------------
// commit mask
// -------------------------------------------------------------------

static inline void pa_commit_mask_create_empty(pa_commit_mask_t* cm) {
  for (size_t i = 0; i < PA_COMMIT_MASK_FIELD_COUNT; i++) {
    cm->mask[i] = 0;
  }
}

static inline void pa_commit_mask_create_full(pa_commit_mask_t* cm) {
  for (size_t i = 0; i < PA_COMMIT_MASK_FIELD_COUNT; i++) {
    cm->mask[i] = ~((size_t)0);
  }
}

static inline bool pa_commit_mask_is_empty(const pa_commit_mask_t* cm) {
  for (size_t i = 0; i < PA_COMMIT_MASK_FIELD_COUNT; i++) {
    if (cm->mask[i] != 0) return false;
  }
  return true;
}

static inline bool pa_commit_mask_is_full(const pa_commit_mask_t* cm) {
  for (size_t i = 0; i < PA_COMMIT_MASK_FIELD_COUNT; i++) {
    if (cm->mask[i] != ~((size_t)0)) return false;
  }
  return true;
}

// defined in `segment.c`:
size_t _pa_commit_mask_committed_size(const pa_commit_mask_t* cm, size_t total);
size_t _pa_commit_mask_next_run(const pa_commit_mask_t* cm, size_t* idx);

#define pa_commit_mask_foreach(cm,idx,count) \
  idx = 0; \
  while ((count = _pa_commit_mask_next_run(cm,&idx)) > 0) {

#define pa_commit_mask_foreach_end() \
    idx += count; \
  }



/* -----------------------------------------------------------
  memory id's
----------------------------------------------------------- */

static inline pa_memid_t _pa_memid_create(pa_memkind_t memkind) {
  pa_memid_t memid;
  _pa_memzero_var(memid);
  memid.memkind = memkind;
  return memid;
}

static inline pa_memid_t _pa_memid_none(void) {
  return _pa_memid_create(PA_MEM_NONE);
}

static inline pa_memid_t _pa_memid_create_os(void* base, size_t size, bool committed, bool is_zero, bool is_large) {
  pa_memid_t memid = _pa_memid_create(PA_MEM_OS);
  memid.mem.os.base = base;
  memid.mem.os.size = size;
  memid.initially_committed = committed;
  memid.initially_zero = is_zero;
  memid.is_pinned = is_large;
  return memid;
}


// -------------------------------------------------------------------
// Fast "random" shuffle
// -------------------------------------------------------------------

static inline uintptr_t _pa_random_shuffle(uintptr_t x) {
  if (x==0) { x = 17; }   // ensure we don't get stuck in generating zeros
#if (PA_INTPTR_SIZE>=8)
  // by Sebastiano Vigna, see: <http://xoshiro.di.unimi.it/splitmix64.c>
  x ^= x >> 30;
  x *= 0xbf58476d1ce4e5b9UL;
  x ^= x >> 27;
  x *= 0x94d049bb133111ebUL;
  x ^= x >> 31;
#elif (PA_INTPTR_SIZE==4)
  // by Chris Wellons, see: <https://nullprogram.com/blog/2018/07/31/>
  x ^= x >> 16;
  x *= 0x7feb352dUL;
  x ^= x >> 15;
  x *= 0x846ca68bUL;
  x ^= x >> 16;
#endif
  return x;
}



// -----------------------------------------------------------------------
// Count bits: trailing or leading zeros (with PA_INTPTR_BITS on all zero)
// -----------------------------------------------------------------------

#if defined(__GNUC__)

#include <limits.h>       // LONG_MAX
#define PA_HAVE_FAST_BITSCAN
static inline size_t pa_clz(size_t x) {
  if (x==0) return PA_SIZE_BITS;
  #if (SIZE_MAX == ULONG_MAX)
    return __builtin_clzl(x);
  #else
    return __builtin_clzll(x);
  #endif
}
static inline size_t pa_ctz(size_t x) {
  if (x==0) return PA_SIZE_BITS;
  #if (SIZE_MAX == ULONG_MAX)
    return __builtin_ctzl(x);
  #else
    return __builtin_ctzll(x);
  #endif
}

#elif defined(_MSC_VER)

#include <limits.h>       // LONG_MAX
#include <intrin.h>       // BitScanReverse64
#define PA_HAVE_FAST_BITSCAN
static inline size_t pa_clz(size_t x) {
  if (x==0) return PA_SIZE_BITS;
  unsigned long idx;
  #if (SIZE_MAX == ULONG_MAX)
    _BitScanReverse(&idx, x);
  #else
    _BitScanReverse64(&idx, x);
  #endif
  return ((PA_SIZE_BITS - 1) - (size_t)idx);
}
static inline size_t pa_ctz(size_t x) {
  if (x==0) return PA_SIZE_BITS;
  unsigned long idx;
  #if (SIZE_MAX == ULONG_MAX)
    _BitScanForward(&idx, x);
  #else
    _BitScanForward64(&idx, x);
  #endif
  return (size_t)idx;
}

#else

static inline size_t pa_ctz_generic32(uint32_t x) {
  // de Bruijn multiplication, see <http://supertech.csail.mit.edu/papers/debruijn.pdf>
  static const uint8_t debruijn[32] = {
    0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
    31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
  };
  if (x==0) return 32;
  return debruijn[(uint32_t)((x & -(int32_t)x) * (uint32_t)(0x077CB531U)) >> 27];
}

static inline size_t pa_clz_generic32(uint32_t x) {
  // de Bruijn multiplication, see <http://supertech.csail.mit.edu/papers/debruijn.pdf>
  static const uint8_t debruijn[32] = {
    31, 22, 30, 21, 18, 10, 29, 2, 20, 17, 15, 13, 9, 6, 28, 1,
    23, 19, 11, 3, 16, 14, 7, 24, 12, 4, 8, 25, 5, 26, 27, 0
  };
  if (x==0) return 32;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return debruijn[(uint32_t)(x * (uint32_t)(0x07C4ACDDU)) >> 27];
}

static inline size_t pa_ctz(size_t x) {
  if (x==0) return PA_SIZE_BITS;
  #if (PA_SIZE_BITS <= 32)
    return pa_ctz_generic32((uint32_t)x);
  #else
    const uint32_t lo = (uint32_t)x;
    if (lo != 0) {
      return pa_ctz_generic32(lo);
    }
    else {
      return (32 + pa_ctz_generic32((uint32_t)(x>>32)));
    }
  #endif
}

static inline size_t pa_clz(size_t x) {
  if (x==0) return PA_SIZE_BITS;
  #if (PA_SIZE_BITS <= 32)
    return pa_clz_generic32((uint32_t)x);
  #else
    const uint32_t hi = (uint32_t)(x>>32);
    if (hi != 0) {
      return pa_clz_generic32(hi);
    }
    else {
      return 32 + pa_clz_generic32((uint32_t)x);
    }
  #endif
}

#endif

// "bit scan reverse": Return index of the highest bit (or PA_SIZE_BITS if `x` is zero)
static inline size_t pa_bsr(size_t x) {
  return (x==0 ? PA_SIZE_BITS : PA_SIZE_BITS - 1 - pa_clz(x));
}

size_t _pa_popcount_generic(size_t x);

static inline size_t pa_popcount(size_t x) {
  if (x<=1) return x;
  if (x==SIZE_MAX) return PA_SIZE_BITS;
  #if defined(__GNUC__)
    #if (SIZE_MAX == ULONG_MAX)
      return __builtin_popcountl(x);
    #else
      return __builtin_popcountll(x);
    #endif
  #else
    return _pa_popcount_generic(x);
  #endif
}

// ---------------------------------------------------------------------------------
// Provide our own `_pa_memcpy` for potential performance optimizations.
//
// For now, only on Windows with msvc/clang-cl we optimize to `rep movsb` if
// we happen to run on x86/x64 cpu's that have "fast short rep movsb" (FSRM) support
// (AMD Zen3+ (~2020) or Intel Ice Lake+ (~2017). See also issue #201 and pr #253.
// ---------------------------------------------------------------------------------

#if !PA_TRACK_ENABLED && (defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__))
#if defined(_WIN32)
#include <intrin.h>
#else
#include <immintrin.h>
static inline void __movsb(uint8_t* dst, const uint8_t* src, size_t n) {
  __asm__ __volatile__("rep movsb" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");
}
static inline void __stosb(uint8_t* dst, uint8_t val, size_t n) {
  __asm__ __volatile__("rep stosb" : "+D"(dst), "+c"(n) : "a"(val) : "memory");
}
#endif
extern pa_decl_hidden bool _pa_cpu_has_fsrm;
extern pa_decl_hidden bool _pa_cpu_has_erms;
extern pa_decl_hidden bool _pa_cpu_has_avx2;

pa_decl_hidden void _pa_memzero_aligned_avx2(void* dst, size_t n);

static inline void _pa_memcpy(void* dst, const void* src, size_t n) {
  if (_pa_cpu_has_fsrm && n <= 127) { // || (_pa_cpu_has_erms && n > 128)) {
    __movsb((unsigned char*)dst, (const unsigned char*)src, n);
  }
  else {
    memcpy(dst, src, n);
  }
}
static inline void _pa_memzero(void* dst, size_t n) {
  if (_pa_cpu_has_fsrm && n <= 127) { // || (_pa_cpu_has_erms && n > 128)) {
    __stosb((unsigned char*)dst, 0, n);
  }
  else {
    memset(dst, 0, n);
  }
}
#else
static inline void _pa_memcpy(void* dst, const void* src, size_t n) {
  memcpy(dst, src, n);
}
static inline void _pa_memzero(void* dst, size_t n) {
  memset(dst, 0, n);
}
#endif

// -------------------------------------------------------------------------------
// The `_pa_memcpy_aligned` can be used if the pointers are machine-word aligned
// This is used for example in `pa_realloc`.
// -------------------------------------------------------------------------------

#if (defined(__GNUC__) && (__GNUC__ >= 4)) || defined(__clang__)
// On GCC/CLang we provide a hint that the pointers are word aligned.
static inline void _pa_memcpy_aligned(void* dst, const void* src, size_t n) {
  pa_assert_internal(((uintptr_t)dst % PA_INTPTR_SIZE == 0) && ((uintptr_t)src % PA_INTPTR_SIZE == 0));
  void* adst = __builtin_assume_aligned(dst, PA_INTPTR_SIZE);
  const void* asrc = __builtin_assume_aligned(src, PA_INTPTR_SIZE);
  _pa_memcpy(adst, asrc, n);
}

static inline void _pa_memzero_aligned(void* dst, size_t n) {
  pa_assert_internal((uintptr_t)dst % PA_INTPTR_SIZE == 0);
  #if (defined(_M_IX86) || defined(_M_X64) || defined(__x86_64__))
  if (_pa_cpu_has_avx2 && n >= 64 && ((uintptr_t)dst % 32 == 0)) {
    _pa_memzero_aligned_avx2(dst, n);
    return;
  }
  #endif
  void* adst = __builtin_assume_aligned(dst, PA_INTPTR_SIZE);
  _pa_memzero(adst, n);
}
#else
// Default fallback on `_pa_memcpy`
static inline void _pa_memcpy_aligned(void* dst, const void* src, size_t n) {
  pa_assert_internal(((uintptr_t)dst % PA_INTPTR_SIZE == 0) && ((uintptr_t)src % PA_INTPTR_SIZE == 0));
  _pa_memcpy(dst, src, n);
}

static inline void _pa_memzero_aligned(void* dst, size_t n) {
  pa_assert_internal((uintptr_t)dst % PA_INTPTR_SIZE == 0);
  #if (defined(_M_IX86) || defined(_M_X64) || defined(__x86_64__))
  if (_pa_cpu_has_avx2 && n >= 64 && ((uintptr_t)dst % 32 == 0)) {
    _pa_memzero_aligned_avx2(dst, n);
    return;
  }
  #endif
  _pa_memzero(dst, n);
}
#endif


#endif
