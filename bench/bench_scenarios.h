/* bench_scenarios.h — Public interface for benchmark scenarios */
#pragma once

#include "bench.h"

/* -------------------------------------------------------------------------
 * Configuration passed to every scenario
 * ---------------------------------------------------------------------- */
typedef struct {
  int    nthreads;     /* number of worker threads */
  size_t iterations;   /* total operations across all threads */
  size_t warmup;       /* warm-up iterations (not timed) */
  size_t size_lo;      /* minimum allocation size (bytes) */
  size_t size_hi;      /* maximum allocation size (bytes) */
} bench_config_t;

/* -------------------------------------------------------------------------
 * Scenario table entry
 * ---------------------------------------------------------------------- */
typedef struct {
  const char*          name;
  void               (*fn)(bench_result_t*, const bench_config_t*);
  const char*          description;
  size_t               default_size_lo;
  size_t               default_size_hi;
} bench_scenario_entry_t;

extern bench_scenario_entry_t g_scenarios[];  /* NULL-terminated */

/* Returns the name of the compiled-in allocator */
const char* bench_alloc_name(void);

/* Individual scenario prototypes */
void bench_alloc_free_same_thread(bench_result_t* r, const bench_config_t* cfg);
void bench_alloc_free_batch      (bench_result_t* r, const bench_config_t* cfg);
void bench_latency_small         (bench_result_t* r, const bench_config_t* cfg);
void bench_latency_large         (bench_result_t* r, const bench_config_t* cfg);
void bench_calloc                (bench_result_t* r, const bench_config_t* cfg);
void bench_realloc               (bench_result_t* r, const bench_config_t* cfg);
void bench_fragmentation_churn   (bench_result_t* r, const bench_config_t* cfg);
void bench_mixed_sizes           (bench_result_t* r, const bench_config_t* cfg);
void bench_alloc_free_mt         (bench_result_t* r, const bench_config_t* cfg);
void bench_cross_thread          (bench_result_t* r, const bench_config_t* cfg);
void bench_thread_scale          (bench_result_t* r, const bench_config_t* cfg);
void bench_object_pool           (bench_result_t* r, const bench_config_t* cfg);
void bench_peak_rss              (bench_result_t* r, const bench_config_t* cfg);
