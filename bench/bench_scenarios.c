/* bench_scenarios.c — Vector-first benchmark scenarios
 * =====================================================
 * All scenarios use the vector arena API only (create, alloc_vector, reset, destroy).
 * No per-vector malloc/free: vector-database-style workloads (float[] embeddings by dimension).
 * Palloc uses the real p_arena_* API; other allocators use a bump-arena shim over their malloc.
 *
 * size_lo / size_hi in config are VECTOR DIMENSIONS (number of floats), not bytes.
 */
#include "bench.h"
#include "bench_scenarios.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>

/* Vector element type (PomaiDB/CheeseBrain: float embeddings) */
#define BENCH_VECTOR_ELEM   float
#define BENCH_VECTOR_ELEMSZ sizeof(BENCH_VECTOR_ELEM)

/* SIMD alignment for vector ops (match PA_ARENA_VECTOR_ALIGN) */
#define BENCH_VECTOR_ALIGN  64

/* =========================================================================
 * Allocator dispatch (backing store for bump arena when not using Palloc)
 * ====================================================================== */
#define sys_malloc(s)    __libc_malloc(s)
#define sys_calloc(n,s)  __libc_calloc(n,s)
#define sys_free(p)      __libc_free(p)
#ifndef __linux__
#  undef sys_malloc
#  undef sys_calloc
#  undef sys_free
#  define sys_malloc(s)   malloc(s)
#  define sys_calloc(n,s) calloc(n,s)
#  define sys_free(p)     free(p)
#endif

#if defined(BENCH_USE_JEMALLOC)
#  include <jemalloc/jemalloc.h>
#  define bench_malloc(s)       je_malloc(s)
#  define bench_calloc(n,s)     je_calloc(n,s)
#  define bench_realloc(p,s)    je_realloc(p,s)
#  define bench_free(p)         je_free(p)
#  define ALLOC_NAME            "jemalloc"

#elif defined(BENCH_USE_MIMALLOC)
#  include <mimalloc.h>
#  define bench_malloc(s)       mi_malloc(s)
#  define bench_calloc(n,s)     mi_calloc(n,s)
#  define bench_realloc(p,s)    mi_realloc(p,s)
#  define bench_free(p)         mi_free(p)
#  define ALLOC_NAME            "mimalloc"

#elif defined(BENCH_USE_PALLOC)
#  include <palloc.h>
#  include "palloc/arena_pomai.h"
#  define bench_malloc(s)       pa_malloc(s)
#  define bench_calloc(n,s)    pa_calloc(n,s)
#  define bench_realloc(p,s)    pa_realloc(p,s)
#  define bench_free(p)         pa_free(p)
#  define ALLOC_NAME            "palloc"

#elif defined(BENCH_USE_TCMALLOC)
#  define bench_malloc(s)       malloc(s)
#  define bench_calloc(n,s)     calloc(n,s)
#  define bench_realloc(p,s)    realloc(p,s)
#  define bench_free(p)         free(p)
#  define ALLOC_NAME            "tcmalloc"

#else
#  define bench_malloc(s)       malloc(s)
#  define bench_calloc(n,s)     calloc(n,s)
#  define bench_realloc(p,s)    realloc(p,s)
#  define bench_free(p)         free(p)
#  define ALLOC_NAME            "system"
#endif

/* =========================================================================
 * Unified vector arena API — all scenarios use this (no per-vector malloc/free)
 * ====================================================================== */
typedef void* bench_arena_t;

#if defined(BENCH_USE_PALLOC)

static inline bench_arena_t bench_arena_create(size_t capacity_bytes) {
  return (bench_arena_t)p_arena_create_for_vector(capacity_bytes);
}
static inline void* bench_arena_alloc_vector(bench_arena_t arena, size_t vector_dim, size_t element_size) {
  return p_arena_alloc_vector((pa_arena_t*)arena, vector_dim, element_size);
}
static inline void bench_arena_reset(bench_arena_t arena) {
  p_arena_reset((pa_arena_t*)arena);
}
static inline void bench_arena_destroy(bench_arena_t arena) {
  p_arena_destroy((pa_arena_t*)arena);
}
static inline bench_arena_t bench_arena_create_shared(size_t capacity_bytes, int shared) {
  return (bench_arena_t)p_arena_create_for_vector_ex(capacity_bytes, (shared != 0));
}

#else

typedef struct bench_bump_arena {
  char*  base;
  size_t capacity;
  size_t used;
} bench_bump_arena_t;

static inline bench_arena_t bench_arena_create(size_t capacity_bytes) {
  size_t total = sizeof(bench_bump_arena_t) + capacity_bytes;
  bench_bump_arena_t* a = (bench_bump_arena_t*)bench_malloc(total);
  if (!a) return NULL;
  a->base = (char*)a + sizeof(bench_bump_arena_t);
  a->capacity = capacity_bytes;
  a->used = 0;
  return (bench_arena_t)a;
}
static inline void* bench_arena_alloc_vector(bench_arena_t arena, size_t vector_dim, size_t element_size) {
  bench_bump_arena_t* a = (bench_bump_arena_t*)arena;
  size_t size = vector_dim * element_size;
  size_t aligned = (size + BENCH_VECTOR_ALIGN - 1) & ~(size_t)(BENCH_VECTOR_ALIGN - 1);
  if (a->used + aligned > a->capacity) return NULL;
  void* p = a->base + a->used;
  a->used += aligned;
  return p;
}
static inline void bench_arena_reset(bench_arena_t arena) {
  ((bench_bump_arena_t*)arena)->used = 0;
}
static inline void bench_arena_destroy(bench_arena_t arena) {
  bench_free(arena);
}
static inline bench_arena_t bench_arena_create_shared(size_t capacity_bytes, int shared) {
  (void)shared;
  return bench_arena_create(capacity_bytes);
}

#endif

static bench_arena_t g_bench_arena;

#define bench_vector_alloc(dim)    bench_arena_alloc_vector(g_bench_arena, (dim), BENCH_VECTOR_ELEMSZ)
#define bench_vector_free(p)       ((void)(p))
#define bench_reset_arena()        bench_arena_reset(g_bench_arena)

/* =========================================================================
 * Helpers
 * ====================================================================== */
/* Simple LCG for fast pseudo-random sizes without stdlib rand() overhead */
static inline uint64_t _lcg(uint64_t* state) {
  *state = *state * 6364136223846793005ULL + 1442695040888963407ULL;
  return *state;
}

static inline size_t _rand_size(uint64_t* rng, size_t lo, size_t hi) {
  if (hi <= lo) return lo;
  return lo + (size_t)(_lcg(rng) % (hi - lo + 1));
}

/* Random vector dimension in [lo, hi] (floats) */
static inline size_t _rand_dim(uint64_t* rng, size_t lo, size_t hi) {
  return _rand_size(rng, lo, hi);
}

/* Touch every page of memory to force RSS */
static inline void _touch(volatile uint8_t* p, size_t sz) {
  for (size_t i = 0; i < sz; i += 4096) p[i] = (uint8_t)i;
}

/* =========================================================================
 * Scenario 1 — alloc_free_same_thread (vector: dim floats, single thread)
 * ====================================================================== */
void bench_alloc_free_same_thread(bench_result_t* r, const bench_config_t* cfg) {
  r->rss_bytes_before = bench_rss_bytes();
  size_t n = cfg->iterations < BENCH_MAX_SAMPLES ? cfg->iterations : BENCH_MAX_SAMPLES;
  uint64_t rng = 0xdeadbeef12345678ULL;

  size_t max_bytes = (size_t)(cfg->size_hi) * BENCH_VECTOR_ELEMSZ * 2; /* 2 vectors per iter */
  size_t capacity = n * max_bytes + (4 * 1024 * 1024);
  g_bench_arena = bench_arena_create(capacity);
  if (!g_bench_arena) { r->n = 0; r->rss_bytes_after = bench_rss_bytes(); return; }

  for (size_t i = 0; i < cfg->warmup; i++) {
    size_t dim = _rand_dim(&rng, cfg->size_lo, cfg->size_hi);
    bench_reset_arena();
    void* p = bench_vector_alloc(dim);
    bench_vector_free(p);
  }

  rng = 0xdeadbeef12345678ULL;
  for (size_t i = 0; i < n; i++) {
    size_t dim = _rand_dim(&rng, cfg->size_lo, cfg->size_hi);
    uint64_t t0 = bench_clock_ns();
    bench_reset_arena();
    void* p = bench_vector_alloc(dim);
    bench_vector_free(p);
    r->samples[i] = bench_clock_ns() - t0;
  }
  r->n = n;
  bench_arena_destroy(g_bench_arena);
  g_bench_arena = NULL;
  r->rss_bytes_after = bench_rss_bytes();
}

/* =========================================================================
 * Scenario 2 — alloc_free_batch (batch of vectors, then reset/free all)
 * ====================================================================== */
#define BATCH_SIZE 4096

void bench_alloc_free_batch(bench_result_t* r, const bench_config_t* cfg) {
  r->rss_bytes_before = bench_rss_bytes();
  void* ptrs[BATCH_SIZE];
  uint64_t rng = 0x1234567890abcdefULL;
  size_t rounds = cfg->iterations / BATCH_SIZE;
  if (rounds == 0) rounds = 1;
  if (rounds > BENCH_MAX_SAMPLES) rounds = BENCH_MAX_SAMPLES;

  size_t max_vec_bytes = (size_t)(cfg->size_hi) * BENCH_VECTOR_ELEMSZ;
  g_bench_arena = bench_arena_create(BATCH_SIZE * max_vec_bytes + 1024 * 1024);
  if (!g_bench_arena) { r->n = 0; r->rss_bytes_after = bench_rss_bytes(); return; }

  for (size_t i = 0; i < BATCH_SIZE; i++)
    ptrs[i] = bench_vector_alloc(_rand_dim(&rng, cfg->size_lo, cfg->size_hi));
  for (size_t i = 0; i < BATCH_SIZE; i++) bench_vector_free(ptrs[i]);
  bench_reset_arena();

  rng = 0x1234567890abcdefULL;
  for (size_t round = 0; round < rounds; round++) {
    uint64_t t0 = bench_clock_ns();
    for (size_t i = 0; i < BATCH_SIZE; i++)
      ptrs[i] = bench_vector_alloc(_rand_dim(&rng, cfg->size_lo, cfg->size_hi));
    for (size_t i = 0; i < BATCH_SIZE; i++) bench_vector_free(ptrs[i]);
    bench_reset_arena();
    r->samples[round] = (bench_clock_ns() - t0) / BATCH_SIZE;
  }
  r->n = rounds;
  bench_arena_destroy(g_bench_arena);
  g_bench_arena = NULL;
  r->rss_bytes_after = bench_rss_bytes();
}

/* =========================================================================
 * Scenario 3 — latency_small (small vector alloc latency, p50/p99)
 * ====================================================================== */
void bench_latency_small(bench_result_t* r, const bench_config_t* cfg) {
  r->rss_bytes_before = bench_rss_bytes();
  size_t n = cfg->iterations < BENCH_MAX_SAMPLES ? cfg->iterations : BENCH_MAX_SAMPLES;
  uint64_t rng = 0xfeed1234cafebabe;

  size_t capacity = n * (size_t)(cfg->size_hi) * BENCH_VECTOR_ELEMSZ * 2 + 1024 * 1024;
  g_bench_arena = bench_arena_create(capacity);
  if (!g_bench_arena) { r->n = 0; r->rss_bytes_after = bench_rss_bytes(); return; }

  for (size_t i = 0; i < cfg->warmup; i++) {
    size_t dim = 8 + (size_t)(_lcg(&rng) % 248);
    void* p = bench_vector_alloc(dim);
    bench_vector_free(p);
    bench_reset_arena();
  }
  for (size_t i = 0; i < n; i++) {
    size_t dim = 8 + (size_t)(_lcg(&rng) % 248);
    uint64_t t0 = bench_clock_ns();
    void* p = bench_vector_alloc(dim);
    uint64_t t1 = bench_clock_ns();
    bench_vector_free(p);
    r->samples[i] = t1 - t0;
  }
  r->n = n;
  bench_arena_destroy(g_bench_arena);
  g_bench_arena = NULL;
  r->rss_bytes_after = bench_rss_bytes();
}

/* =========================================================================
 * Scenario 4 — latency_large (1 MiB vector = 262144 floats)
 * ====================================================================== */
#define LATENCY_LARGE_DIM  (1024 * 1024 / BENCH_VECTOR_ELEMSZ)  /* 262144 */

void bench_latency_large(bench_result_t* r, const bench_config_t* cfg) {
  (void)cfg;
  r->rss_bytes_before = bench_rss_bytes();
  size_t n = (cfg->iterations / 100);
  if (n == 0) n = 100;
  if (n > BENCH_MAX_SAMPLES) n = BENCH_MAX_SAMPLES;
  size_t dim = LATENCY_LARGE_DIM;

  g_bench_arena = bench_arena_create(n * dim * BENCH_VECTOR_ELEMSZ + 2 * 1024 * 1024);
  if (!g_bench_arena) { r->n = 0; r->rss_bytes_after = bench_rss_bytes(); return; }

  for (size_t i = 0; i < 10; i++) {
    void* p = bench_vector_alloc(dim);
    bench_vector_free(p);
    bench_reset_arena();
  }
  for (size_t i = 0; i < n; i++) {
    uint64_t t0 = bench_clock_ns();
    void* p = bench_vector_alloc(dim);
    uint64_t t1 = bench_clock_ns();
    if (p) { _touch((volatile uint8_t*)p, dim * BENCH_VECTOR_ELEMSZ); }
    bench_vector_free(p);
    r->samples[i] = t1 - t0;
  }
  r->n = n;
  bench_arena_destroy(g_bench_arena);
  g_bench_arena = NULL;
  r->rss_bytes_after = bench_rss_bytes();
}

/* =========================================================================
 * Scenario 5 — calloc_bench (zeroed vector alloc: dim floats)
 * ====================================================================== */
void bench_calloc_scenario(bench_result_t* r, const bench_config_t* cfg) {
  r->rss_bytes_before = bench_rss_bytes();
  size_t n = cfg->iterations < BENCH_MAX_SAMPLES ? cfg->iterations : BENCH_MAX_SAMPLES;
  uint64_t rng = 0xabcd1234;

  size_t capacity = n * (size_t)(cfg->size_hi) * BENCH_VECTOR_ELEMSZ * 2 + 1024 * 1024;
  g_bench_arena = bench_arena_create(capacity);
  if (!g_bench_arena) { r->n = 0; r->rss_bytes_after = bench_rss_bytes(); return; }

  for (size_t i = 0; i < n; i++) {
    size_t dim = _rand_dim(&rng, cfg->size_lo, cfg->size_hi);
    uint64_t t0 = bench_clock_ns();
    void* p = bench_vector_alloc(dim);
    r->samples[i] = bench_clock_ns() - t0;
    bench_vector_free(p);
  }
  r->n = n;
  bench_arena_destroy(g_bench_arena);
  g_bench_arena = NULL;
  r->rss_bytes_after = bench_rss_bytes();
}

/* =========================================================================
 * Scenario 6 — realloc_bench (grow/shrink vector: new dim each time)
 * Arena: no realloc; we alloc new size (old effectively abandoned until reset).
 * ====================================================================== */
void bench_realloc_scenario(bench_result_t* r, const bench_config_t* cfg) {
  r->rss_bytes_before = bench_rss_bytes();
  size_t n = cfg->iterations < BENCH_MAX_SAMPLES ? cfg->iterations : BENCH_MAX_SAMPLES;
  uint64_t rng = 0x7654321abcdef;

  size_t capacity = n * (size_t)(cfg->size_hi) * BENCH_VECTOR_ELEMSZ + 2 * 1024 * 1024;
  g_bench_arena = bench_arena_create(capacity);
  if (!g_bench_arena) { r->n = 0; r->rss_bytes_after = bench_rss_bytes(); return; }

  void* p = bench_vector_alloc(cfg->size_lo);
  for (size_t i = 0; i < n; i++) {
    size_t dim = _rand_dim(&rng, cfg->size_lo, cfg->size_hi);
    uint64_t t0 = bench_clock_ns();
    void* q = bench_vector_alloc(dim);
    r->samples[i] = bench_clock_ns() - t0;
    if (q) { p = q; }
  }
  bench_vector_free(p);
  r->n = n;
  bench_arena_destroy(g_bench_arena);
  g_bench_arena = NULL;
  r->rss_bytes_after = bench_rss_bytes();
}

/* =========================================================================
 * Scenario 7 — fragmentation_churn (working set of N vectors, randomly replace)
 * PALLOC: reset every FRAG_SLOTS iters, then alloc one (no per-slot free).
 * ====================================================================== */
#define FRAG_SLOTS 65536

void bench_fragmentation_churn(bench_result_t* r, const bench_config_t* cfg) {
  r->rss_bytes_before = bench_rss_bytes();
  void** ptrs = (void**)malloc(FRAG_SLOTS * sizeof(void*));
  memset(ptrs, 0, FRAG_SLOTS * sizeof(void*));
  uint64_t rng = 0x1111222233334444ULL;
  size_t n = cfg->iterations < BENCH_MAX_SAMPLES ? cfg->iterations : BENCH_MAX_SAMPLES;

  size_t max_vec = (size_t)(cfg->size_hi) * BENCH_VECTOR_ELEMSZ;
  g_bench_arena = bench_arena_create(FRAG_SLOTS * max_vec + 4 * 1024 * 1024);
  if (!g_bench_arena) { free(ptrs); r->n = 0; r->rss_bytes_after = bench_rss_bytes(); return; }

  for (int i = 0; i < FRAG_SLOTS; i++)
    ptrs[i] = bench_vector_alloc(_rand_dim(&rng, cfg->size_lo, cfg->size_hi));

  for (size_t i = 0; i < n; i++) {
    if (i % FRAG_SLOTS == 0 && i > 0)
      bench_reset_arena();
    size_t idx = (size_t)(_lcg(&rng) % FRAG_SLOTS);
    bench_vector_free(ptrs[idx]);
    size_t dim = _rand_dim(&rng, cfg->size_lo, cfg->size_hi);
    uint64_t t0 = bench_clock_ns();
    ptrs[idx] = bench_vector_alloc(dim);
    r->samples[i] = bench_clock_ns() - t0;
  }

  for (int i = 0; i < FRAG_SLOTS; i++) bench_vector_free(ptrs[i]);
  bench_arena_destroy(g_bench_arena);
  g_bench_arena = NULL;
  free(ptrs);
  r->n = n;
  r->rss_bytes_after = bench_rss_bytes();
}

/* =========================================================================
 * Scenario 8 — mixed_sizes (vector dims: 60% small, 30% medium, 10% large)
 * ====================================================================== */
void bench_mixed_sizes(bench_result_t* r, const bench_config_t* cfg) {
  (void)cfg;
  r->rss_bytes_before = bench_rss_bytes();
  size_t n = cfg->iterations < BENCH_MAX_SAMPLES ? cfg->iterations : BENCH_MAX_SAMPLES;
  uint64_t rng = 0x9988776655443322ULL;

  size_t capacity = n * (1024 * 1024 / BENCH_VECTOR_ELEMSZ) * BENCH_VECTOR_ELEMSZ + 8 * 1024 * 1024;
  g_bench_arena = bench_arena_create(capacity);
  if (!g_bench_arena) { r->n = 0; r->rss_bytes_after = bench_rss_bytes(); return; }

  /* dim distribution: 60% 8-256, 30% 256-16K, 10% 16K-256K floats */
  for (size_t i = 0; i < n; i++) {
    uint64_t roll = _lcg(&rng) % 100;
    size_t dim;
    if (roll < 60)      dim = _rand_dim(&rng, 8, 256);
    else if (roll < 90) dim = _rand_dim(&rng, 256, 16384);
    else                dim = _rand_dim(&rng, 16384, 262144);

    uint64_t t0 = bench_clock_ns();
    void* p = bench_vector_alloc(dim);
    bench_vector_free(p);
    r->samples[i] = bench_clock_ns() - t0;
  }
  r->n = n;
  bench_arena_destroy(g_bench_arena);
  g_bench_arena = NULL;
  r->rss_bytes_after = bench_rss_bytes();
}

/* =========================================================================
 * Scenario 9..N+N — multithreaded scenarios
 * ====================================================================== */
typedef struct {
  int           thread_id;
  int           nthreads;
  size_t        size_lo;
  size_t        size_hi;
  size_t        iterations;
  size_t        warmup;
  uint64_t*     samples;
  size_t        n_samples;
  void*         arena;  /* BENCH_USE_PALLOC: per-thread pa_arena_t* */
  void**        shared_queue;
  int           queue_size;
  atomic_int*   queue_head;
  atomic_int*   queue_tail;
  atomic_int*   done_flag;
} thread_arg_t;

static void* _mt_alloc_free_worker(void* arg) {
  thread_arg_t* a = (thread_arg_t*)arg;
  bench_arena_t ar = (bench_arena_t)a->arena;
  uint64_t rng = 0xbabebabe ^ (uint64_t)a->thread_id;
  size_t max_n = a->iterations < BENCH_MAX_SAMPLES ? a->iterations : BENCH_MAX_SAMPLES;

  for (size_t i = 0; i < a->warmup; i++) {
    size_t dim = _rand_dim(&rng, a->size_lo, a->size_hi);
    bench_arena_reset(ar);
    { void* _p = bench_arena_alloc_vector(ar, dim, BENCH_VECTOR_ELEMSZ); (void)_p; }
  }
  for (size_t i = 0; i < max_n; i++) {
    size_t dim = _rand_dim(&rng, a->size_lo, a->size_hi);
    uint64_t t0 = bench_clock_ns();
    bench_arena_reset(ar);
    { void* _p = bench_arena_alloc_vector(ar, dim, BENCH_VECTOR_ELEMSZ); (void)_p; }
    a->samples[i] = bench_clock_ns() - t0;
  }
  a->n_samples = max_n;
  return NULL;
}

static void _run_threads(bench_result_t* r, const bench_config_t* cfg,
                          void* (*worker)(void*)) {
  int nt = cfg->nthreads;
  pthread_t* tids         = (pthread_t*)       malloc((size_t)nt * sizeof(pthread_t));
  thread_arg_t* args     = (thread_arg_t*)   malloc((size_t)nt * sizeof(thread_arg_t));
  uint64_t** thread_samples = (uint64_t**)  malloc((size_t)nt * sizeof(uint64_t*));
  bench_arena_t* arenas = (bench_arena_t*) malloc((size_t)nt * sizeof(bench_arena_t));
  size_t per_thread_iters = cfg->iterations / (size_t)nt;
  size_t max_vec_bytes    = (size_t)(cfg->size_hi) * BENCH_VECTOR_ELEMSZ * 2;
  size_t arena_cap        = per_thread_iters * max_vec_bytes + 4 * 1024 * 1024;

  r->rss_bytes_before = bench_rss_bytes();

  for (int i = 0; i < nt; i++) {
    thread_samples[i] = (uint64_t*)malloc(BENCH_MAX_SAMPLES * sizeof(uint64_t));
    args[i].thread_id   = i;
    args[i].nthreads    = nt;
    args[i].size_lo     = cfg->size_lo;
    args[i].size_hi     = cfg->size_hi;
    args[i].iterations  = cfg->iterations / (size_t)nt;
    args[i].warmup      = cfg->warmup / (size_t)nt;
    args[i].samples     = thread_samples[i];
    args[i].n_samples   = 0;
    arenas[i]           = bench_arena_create(arena_cap);
    args[i].arena       = arenas[i];
    pthread_create(&tids[i], NULL, worker, &args[i]);
  }

  size_t total_n = 0;
  for (int i = 0; i < nt; i++) {
    pthread_join(tids[i], NULL);
    size_t n = args[i].n_samples;
    if (total_n + n > BENCH_MAX_SAMPLES) n = BENCH_MAX_SAMPLES - total_n;
    memcpy(r->samples + total_n, thread_samples[i], n * sizeof(uint64_t));
    total_n += n;
    free(thread_samples[i]);
    if (arenas[i]) bench_arena_destroy(arenas[i]);
  }
  r->n = total_n;
  r->rss_bytes_after = bench_rss_bytes();

  free(tids);
  free(args);
  free(thread_samples);
  free(arenas);
}

void bench_alloc_free_mt(bench_result_t* r, const bench_config_t* cfg) {
  _run_threads(r, cfg, _mt_alloc_free_worker);
}

/* =========================================================================
 * Scenario — cross_thread (producer/consumer ring)
 * Producer allocs and pushes to a ring; consumer pops and frees.
 * ====================================================================== */
#define RING_SIZE 8192

typedef struct {
  void**      ring;
  atomic_int  head;
  atomic_int  tail;
  atomic_int  done;
  size_t      size_lo, size_hi;
  size_t      n_iter;
  uint64_t*   samples;
  size_t      n_samples;
  void*       arena;  /* BENCH_USE_PALLOC: shared pa_arena_t* */
} ring_ctx_t;

static void* _producer(void* arg) {
  ring_ctx_t* ctx = (ring_ctx_t*)arg;
  uint64_t rng = 0xdeadcafe;
  for (size_t i = 0; i < ctx->n_iter; i++) {
    size_t dim = _rand_dim(&rng, ctx->size_lo, ctx->size_hi);
    void* p = ctx->arena ? bench_arena_alloc_vector((bench_arena_t)ctx->arena, dim, BENCH_VECTOR_ELEMSZ) : NULL;
    int tail;
    do { tail = atomic_load_explicit(&ctx->tail, memory_order_relaxed); }
    while (((tail + 1) % RING_SIZE) == atomic_load_explicit(&ctx->head, memory_order_acquire));
    ctx->ring[tail] = p;
    atomic_store_explicit(&ctx->tail, (tail + 1) % RING_SIZE, memory_order_release);
  }
  atomic_store(&ctx->done, 1);
  return NULL;
}

static void* _consumer(void* arg) {
  ring_ctx_t* ctx = (ring_ctx_t*)arg;
  size_t freed = 0;
  size_t n_iter = ctx->n_iter;
  size_t max_s = BENCH_MAX_SAMPLES;
  while (freed < n_iter) {
    int head = atomic_load_explicit(&ctx->head, memory_order_relaxed);
    int tail = atomic_load_explicit(&ctx->tail, memory_order_acquire);
    if (head == tail) {
      if (atomic_load(&ctx->done)) break;
      continue;
    }
    void* p = ctx->ring[head];
    atomic_store_explicit(&ctx->head, (head + 1) % RING_SIZE, memory_order_release);
    uint64_t t0 = bench_clock_ns();
    (void)p;  /* vector arena: no per-block free; reset at end */
    if (freed < max_s) ctx->samples[freed] = bench_clock_ns() - t0;
    freed++;
  }
  ctx->n_samples = freed < max_s ? freed : max_s;
  return NULL;
}

void bench_cross_thread(bench_result_t* r, const bench_config_t* cfg) {
  r->rss_bytes_before = bench_rss_bytes();
  ring_ctx_t ctx = {0};
  ctx.ring     = (void**)malloc(RING_SIZE * sizeof(void*));
  memset(ctx.ring, 0, RING_SIZE * sizeof(void*));
  ctx.size_lo  = cfg->size_lo;
  ctx.size_hi  = cfg->size_hi;
  ctx.n_iter   = cfg->iterations < 500000 ? cfg->iterations : 500000;
  ctx.samples  = r->samples;
  atomic_init(&ctx.head, 0);
  atomic_init(&ctx.tail, 0);
  atomic_init(&ctx.done, 0);

  size_t max_vec = (size_t)(cfg->size_hi) * BENCH_VECTOR_ELEMSZ;
  ctx.arena = bench_arena_create_shared(ctx.n_iter * max_vec + 1024 * 1024, 1);

  pthread_t prod_tid, cons_tid;
  pthread_create(&prod_tid, NULL, _producer, &ctx);
  pthread_create(&cons_tid, NULL, _consumer, &ctx);
  pthread_join(prod_tid, NULL);
  pthread_join(cons_tid, NULL);

  if (ctx.arena) bench_arena_destroy((bench_arena_t)ctx.arena);
  r->n = ctx.n_samples;
  r->rss_bytes_after = bench_rss_bytes();
  free(ctx.ring);
}

/* =========================================================================
 * Scenario — thread_scale_throughput
 * Single-thread through 32-thread throughput (one iteration per thread count)
 * ====================================================================== */
void bench_thread_scale(bench_result_t* r, const bench_config_t* cfg) {
  /* Overloads: each "sample" is one thread-count configuration.
     We store throughput as inverse latency (total_ns / n_ops) */
  int thread_counts[] = {1, 2, 4, 8, 16, 32};
  int nc = 6;
  r->rss_bytes_before = bench_rss_bytes();

  bench_config_t c2 = *cfg;
  for (int t = 0; t < nc; t++) {
    c2.nthreads = thread_counts[t];
    uint64_t* tmp_s = (uint64_t*)malloc(BENCH_MAX_SAMPLES * sizeof(uint64_t));
    bench_result_t tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.samples = tmp_s;
    _run_threads(&tmp, &c2, _mt_alloc_free_worker);
    bench_stats_compute(&tmp);
    /* Store p50 latency as proxy — caller can interpret */
    if ((size_t)t < BENCH_MAX_SAMPLES) r->samples[t] = tmp.p50_ns;
    free(tmp_s);
  }
  r->n = (size_t)nc;
  r->rss_bytes_after = bench_rss_bytes();
}

/* =========================================================================
 * Scenario — object_pool (fixed small vector = 16 floats = 64B)
 * ====================================================================== */
#define POOL_VECTOR_DIM  (64 / BENCH_VECTOR_ELEMSZ)  /* 16 floats */

static void* _pool_worker(void* arg) {
  thread_arg_t* a = (thread_arg_t*)arg;
  bench_arena_t ar = (bench_arena_t)a->arena;
  size_t dim = POOL_VECTOR_DIM;
  size_t n = a->iterations;
  if (n > BENCH_MAX_SAMPLES) n = BENCH_MAX_SAMPLES;

  for (size_t i = 0; i < a->warmup; i++) {
    bench_arena_reset(ar);
    { void* _p = bench_arena_alloc_vector(ar, dim, BENCH_VECTOR_ELEMSZ); (void)_p; }
  }
  for (size_t i = 0; i < n; i++) {
    uint64_t t0 = bench_clock_ns();
    bench_arena_reset(ar);
    { void* _p = bench_arena_alloc_vector(ar, dim, BENCH_VECTOR_ELEMSZ); (void)_p; }
    a->samples[i] = bench_clock_ns() - t0;
  }
  a->n_samples = n;
  return NULL;
}

void bench_object_pool(bench_result_t* r, const bench_config_t* cfg) {
  _run_threads(r, cfg, _pool_worker);
}

/* =========================================================================
 * Scenario — peak_rss (many vectors, touch, measure RSS, then reset/free)
 * ====================================================================== */
void bench_peak_rss(bench_result_t* r, const bench_config_t* cfg) {
  const size_t N = 65536;
  void** ptrs = (void**)malloc(N * sizeof(void*));
  memset(ptrs, 0, N * sizeof(void*));
  uint64_t rng = 0xdeadbeef;
  r->rss_bytes_before = bench_rss_bytes();

  size_t max_vec = (size_t)(cfg->size_hi) * BENCH_VECTOR_ELEMSZ;
  g_bench_arena = bench_arena_create(N * max_vec + 4 * 1024 * 1024);
  if (!g_bench_arena) { free(ptrs); r->n = 0; r->rss_bytes_after = bench_rss_bytes(); return; }

  for (size_t i = 0; i < N; i++) {
    size_t dim = _rand_dim(&rng, cfg->size_lo, cfg->size_hi);
    ptrs[i] = bench_vector_alloc(dim);
    if (ptrs[i]) _touch((volatile uint8_t*)ptrs[i], dim * BENCH_VECTOR_ELEMSZ);
  }
  long rss_peak = bench_rss_bytes();

  uint64_t t0 = bench_clock_ns();
  for (size_t i = 0; i < N; i++) bench_vector_free(ptrs[i]);
  bench_reset_arena();
  bench_arena_destroy(g_bench_arena);
  g_bench_arena = NULL;
  r->samples[0] = bench_clock_ns() - t0;
  free(ptrs);

  r->n = 1;
  r->rss_bytes_after = rss_peak;
}

/* =========================================================================
 * Scenario table (exported)
 * size_lo/size_hi = vector dimensions (number of floats)
 * ====================================================================== */
bench_scenario_entry_t g_scenarios[] = {
  { "alloc_free_same_thread",  bench_alloc_free_same_thread, "Single-thread vector alloc+reset (8-256 dim)",    8,    256    },
  { "alloc_free_batch",        bench_alloc_free_batch,       "Batch vector alloc then reset (8-256 dim)",      8,    256    },
  { "latency_small",           bench_latency_small,           "Vector alloc latency p50/p99 (8-256 dim)",        8,    256    },
  { "latency_large",           bench_latency_large,           "Vector alloc latency (1 MiB = 262144 dim)",      262144, 262144 },
  { "calloc_bench",            bench_calloc_scenario,         "Zeroed vector alloc (256-64K dim)",              256,  65536   },
  { "realloc_bench",           bench_realloc_scenario,        "Vector grow/shrink (16-64K dim)",                 16,   65536   },
  { "fragmentation_churn",     bench_fragmentation_churn,     "Vector churn N slots (8-512 dim)",                8,    512     },
  { "mixed_sizes",             bench_mixed_sizes,             "Mixed vector dims (small/med/large)",             8,    262144  },
  { "alloc_free_mt",           bench_alloc_free_mt,            "Multi-thread vector alloc+reset (8-256 dim)",    8,    256     },
  { "cross_thread",            bench_cross_thread,            "Cross-thread vector producer/consumer",         8,    256     },
  { "thread_scale",            bench_thread_scale,             "Thread scaling 1->32 (vector p50)",              8,    256     },
  { "object_pool",             bench_object_pool,              "Fixed vector 16 floats / 64B",                    16,   16      },
  { "peak_rss",                bench_peak_rss,                 "Peak RSS N vectors (256-64K dim)",               256,  65536   },
  { NULL, NULL, NULL, 0, 0 }
};

const char* bench_alloc_name(void) { return ALLOC_NAME; }
