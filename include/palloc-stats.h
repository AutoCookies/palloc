/* ----------------------------------------------------------------------------
Copyright (c) 2018-2025, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#pragma once
#ifndef PALLOC_STATS_H
#define PALLOC_STATS_H

#include <palloc.h>
#include <stdint.h>

#define PA_STAT_VERSION   4  // increased on every backward incompatible change

// count allocation over time
typedef struct pa_stat_count_s {
  int64_t total;                              // total allocated
  int64_t peak;                               // peak allocation
  int64_t current;                            // current allocation
} pa_stat_count_t;

// counters only increase
typedef struct pa_stat_counter_s {
  int64_t total;                              // total count
} pa_stat_counter_t;

#define PA_STAT_FIELDS() \
  PA_STAT_COUNT(pages)                      /* count of palloc pages */ \
  PA_STAT_COUNT(reserved)                   /* reserved memory bytes */ \
  PA_STAT_COUNT(committed)                  /* committed bytes */ \
  PA_STAT_COUNTER(reset)                    /* reset bytes */ \
  PA_STAT_COUNTER(purged)                   /* purged bytes */ \
  PA_STAT_COUNT(page_committed)             /* committed memory inside pages */ \
  PA_STAT_COUNT(pages_abandoned)            /* abandonded pages count */ \
  PA_STAT_COUNT(threads)                    /* number of threads */ \
  PA_STAT_COUNT(malloc_normal)              /* allocated bytes <= PA_LARGE_OBJ_SIZE_MAX */ \
  PA_STAT_COUNT(malloc_huge)                /* allocated bytes in huge pages */ \
  PA_STAT_COUNT(malloc_requested)           /* malloc requested bytes */ \
  \
  PA_STAT_COUNTER(mmap_calls) \
  PA_STAT_COUNTER(commit_calls) \
  PA_STAT_COUNTER(reset_calls) \
  PA_STAT_COUNTER(purge_calls) \
  PA_STAT_COUNTER(arena_count)              /* number of memory arena's */ \
  PA_STAT_COUNTER(malloc_normal_count)      /* number of blocks <= PA_LARGE_OBJ_SIZE_MAX */ \
  PA_STAT_COUNTER(malloc_huge_count)        /* number of huge bloks */ \
  PA_STAT_COUNTER(malloc_guarded_count)     /* number of allocations with guard pages */ \
  \
  /* internal statistics */ \
  PA_STAT_COUNTER(arena_rollback_count) \
  PA_STAT_COUNTER(arena_purges) \
  PA_STAT_COUNTER(pages_extended)           /* number of page extensions */ \
  PA_STAT_COUNTER(pages_retire)             /* number of pages that are retired */ \
  PA_STAT_COUNTER(page_searches)            /* total pages searched for a fresh page */ \
  PA_STAT_COUNTER(page_searches_count)      /* searched count for a fresh page */ \
  /* only on v1 and v2 */ \
  PA_STAT_COUNT(segments) \
  PA_STAT_COUNT(segments_abandoned) \
  PA_STAT_COUNT(segments_cache) \
  PA_STAT_COUNT(_segments_reserved) \
  /* only on v3 */ \
  PA_STAT_COUNTER(pages_reclaim_on_alloc) \
  PA_STAT_COUNTER(pages_reclaim_on_free) \
  PA_STAT_COUNTER(pages_reabandon_full) \
  PA_STAT_COUNTER(pages_unabandon_busy_wait) \


// Define the statistics structure
#define PA_BIN_HUGE             (73U)   // see types.h
#define PA_STAT_COUNT(stat)     pa_stat_count_t stat;
#define PA_STAT_COUNTER(stat)   pa_stat_counter_t stat;

typedef struct pa_stats_s
{
  size_t size;          // size of the pa_stats_t structure 
  size_t version;       

  PA_STAT_FIELDS()

  // future extension
  pa_stat_count_t   _stat_reserved[4];
  pa_stat_counter_t _stat_counter_reserved[4];

  // size segregated statistics
  pa_stat_count_t   malloc_bins[PA_BIN_HUGE+1];   // allocation per size bin
  pa_stat_count_t   page_bins[PA_BIN_HUGE+1];     // pages allocated per size bin
} pa_stats_t;

#undef PA_STAT_COUNT
#undef PA_STAT_COUNTER

// helper
#define pa_stats_t_decl(name)  pa_stats_t name = { 0 }; name.size = sizeof(pa_stats_t); name.version = PA_STAT_VERSION;

// Exported definitions
#ifdef __cplusplus
extern "C" {
#endif

pa_decl_export bool  pa_stats_get( pa_stats_t* stats ) pa_attr_noexcept;
pa_decl_export char* pa_stats_get_json( size_t buf_size, char* buf ) pa_attr_noexcept;    // use pa_free to free the result if the input buf == NULL

#ifdef __cplusplus
}
#endif

#endif // PALLOC_STATS_H
