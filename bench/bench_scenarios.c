/* bench_scenarios.c — All benchmark scenarios
 * =============================================
 * Each scenario exports a function:
 *   void bench_<name>(bench_result_t* r, const bench_config_t* cfg)
 *
 * The function fills r->samples[0..n-1] with per-operation latency in ns,
 * sets r->n, r->rss_bytes_before, r->rss_bytes_after.
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

/* =========================================================================
 * Allocator dispatch
 * ====================================================================== */
/* Use aliased names from stdlib to avoid collision with bench_* macros below */
#define sys_malloc(s)    __libc_malloc(s)
#define sys_calloc(n,s)  __libc_calloc(n,s)
#define sys_free(p)      __libc_free(p)
/* On non-glibc, fall back to regular stdlib */
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
#  define bench_malloc(s)       pa_malloc(s)
#  define bench_calloc(n,s)     pa_calloc(n,s)
#  define bench_realloc(p,s)    pa_realloc(p,s)
#  define bench_free(p)         pa_free(p)
#  define ALLOC_NAME            "palloc"

#elif defined(BENCH_USE_TCMALLOC)
   /* tcmalloc overrides malloc globally via LD_PRELOAD or linking;
      use standard symbols */
#  define bench_malloc(s)       malloc(s)
#  define bench_calloc(n,s)     calloc(n,s)
#  define bench_realloc(p,s)    realloc(p,s)
#  define bench_free(p)         free(p)
#  define ALLOC_NAME            "tcmalloc"

#else  /* system malloc */
#  define bench_malloc(s)       malloc(s)
#  define bench_calloc(n,s)     calloc(n,s)
#  define bench_realloc(p,s)    realloc(p,s)
#  define bench_free(p)         free(p)
#  define ALLOC_NAME            "system"
#endif

/* =========================================================================
 * Helpers
 * ====================================================================== */
/* Simple LCG for fast pseudo-random sizes without stdlib rand() overhead */
static inline uint64_t _lcg(uint64_t* state) {
  *state = *state * 6364136223846793005ULL + 1442695040888963407ULL;
  return *state;
}

static inline size_t _rand_size(uint64_t* rng, size_t lo, size_t hi) {
  return lo + (size_t)(_lcg(rng) % (hi - lo + 1));
}

/* Touch every page of memory to force RSS */
static inline void _touch(volatile uint8_t* p, size_t sz) {
  for (size_t i = 0; i < sz; i += 4096) p[i] = (uint8_t)i;
}

/* =========================================================================
 * Scenario 1 — alloc_free_same_thread (small, single thread)
 * ====================================================================== */
void bench_alloc_free_same_thread(bench_result_t* r, const bench_config_t* cfg) {
  r->rss_bytes_before = bench_rss_bytes();
  size_t n = cfg->iterations < BENCH_MAX_SAMPLES ? cfg->iterations : BENCH_MAX_SAMPLES;
  uint64_t rng = 0xdeadbeef12345678ULL;

  /* Warmup */
  for (size_t i = 0; i < cfg->warmup; i++) {
    void* p = bench_malloc(_rand_size(&rng, cfg->size_lo, cfg->size_hi));
    bench_free(p);
  }

  rng = 0xdeadbeef12345678ULL;
  for (size_t i = 0; i < n; i++) {
    size_t sz = _rand_size(&rng, cfg->size_lo, cfg->size_hi);
    uint64_t t0 = bench_clock_ns();
    void* p = bench_malloc(sz);
    bench_free(p);
    r->samples[i] = bench_clock_ns() - t0;
  }
  r->n = n;
  r->rss_bytes_after = bench_rss_bytes();
}

/* =========================================================================
 * Scenario 2 — alloc_free_batch (allocate many, then free all)
 * ====================================================================== */
#define BATCH_SIZE 4096

void bench_alloc_free_batch(bench_result_t* r, const bench_config_t* cfg) {
  r->rss_bytes_before = bench_rss_bytes();
  void* ptrs[BATCH_SIZE];
  uint64_t rng = 0x1234567890abcdefULL;
  size_t rounds = cfg->iterations / BATCH_SIZE;
  if (rounds == 0) rounds = 1;
  if (rounds > BENCH_MAX_SAMPLES) rounds = BENCH_MAX_SAMPLES;

  /* Warmup */
  for (size_t i = 0; i < BATCH_SIZE; i++) {
    ptrs[i] = bench_malloc(_rand_size(&rng, cfg->size_lo, cfg->size_hi));
  }
  for (size_t i = 0; i < BATCH_SIZE; i++) bench_free(ptrs[i]);

  rng = 0x1234567890abcdefULL;
  for (size_t round = 0; round < rounds; round++) {
    uint64_t t0 = bench_clock_ns();
    for (size_t i = 0; i < BATCH_SIZE; i++) {
      ptrs[i] = bench_malloc(_rand_size(&rng, cfg->size_lo, cfg->size_hi));
    }
    for (size_t i = 0; i < BATCH_SIZE; i++) bench_free(ptrs[i]);
    r->samples[round] = (bench_clock_ns() - t0) / BATCH_SIZE;
  }
  r->n = rounds;
  r->rss_bytes_after = bench_rss_bytes();
}

/* =========================================================================
 * Scenario 3 — latency_small (individual small-object latency with p99)
 * ====================================================================== */
void bench_latency_small(bench_result_t* r, const bench_config_t* cfg) {
  r->rss_bytes_before = bench_rss_bytes();
  size_t n = cfg->iterations < BENCH_MAX_SAMPLES ? cfg->iterations : BENCH_MAX_SAMPLES;
  uint64_t rng = 0xfeed1234cafebabe;

  for (size_t i = 0; i < cfg->warmup; i++) {
    void* p = bench_malloc(16 + (_lcg(&rng) % 240));
    bench_free(p);
  }
  for (size_t i = 0; i < n; i++) {
    size_t sz = 8 + (size_t)(_lcg(&rng) % 248);
    uint64_t t0 = bench_clock_ns();
    void* p = bench_malloc(sz);
    uint64_t t1 = bench_clock_ns();
    bench_free(p);
    r->samples[i] = t1 - t0;
  }
  r->n = n;
  r->rss_bytes_after = bench_rss_bytes();
}

/* =========================================================================
 * Scenario 4 — latency_large (1 MiB allocs)
 * ====================================================================== */
void bench_latency_large(bench_result_t* r, const bench_config_t* cfg) {
  r->rss_bytes_before = bench_rss_bytes();
  size_t n = (cfg->iterations / 100);
  if (n == 0) n = 100;
  if (n > BENCH_MAX_SAMPLES) n = BENCH_MAX_SAMPLES;
  size_t sz = 1024 * 1024;

  for (size_t i = 0; i < 10; i++) {
    void* p = bench_malloc(sz); bench_free(p);
  }
  for (size_t i = 0; i < n; i++) {
    uint64_t t0 = bench_clock_ns();
    void* p = bench_malloc(sz);
    uint64_t t1 = bench_clock_ns();
    if (p) { _touch((volatile uint8_t*)p, sz); }
    bench_free(p);
    r->samples[i] = t1 - t0;
  }
  r->n = n;
  r->rss_bytes_after = bench_rss_bytes();
}

/* =========================================================================
 * Scenario 5 — calloc_bench (zeroed allocation throughput, SIMD path)
 * ====================================================================== */
void bench_calloc_scenario(bench_result_t* r, const bench_config_t* cfg) {
  r->rss_bytes_before = bench_rss_bytes();
  size_t n = cfg->iterations < BENCH_MAX_SAMPLES ? cfg->iterations : BENCH_MAX_SAMPLES;
  uint64_t rng = 0xabcd1234;

  for (size_t i = 0; i < n; i++) {
    size_t sz = _rand_size(&rng, cfg->size_lo, cfg->size_hi);
    uint64_t t0 = bench_clock_ns();
    void* p = bench_calloc(1, sz);
    r->samples[i] = bench_clock_ns() - t0;
    bench_free(p);
  }
  r->n = n;
  r->rss_bytes_after = bench_rss_bytes();
}

/* =========================================================================
 * Scenario 6 — realloc_bench
 * ====================================================================== */
void bench_realloc_scenario(bench_result_t* r, const bench_config_t* cfg) {
  r->rss_bytes_before = bench_rss_bytes();
  size_t n = cfg->iterations < BENCH_MAX_SAMPLES ? cfg->iterations : BENCH_MAX_SAMPLES;
  uint64_t rng = 0x7654321abcdef;

  void* p = bench_malloc(cfg->size_lo);
  for (size_t i = 0; i < n; i++) {
    size_t sz = _rand_size(&rng, cfg->size_lo, cfg->size_hi);
    uint64_t t0 = bench_clock_ns();
    void* q = bench_realloc(p, sz);
    r->samples[i] = bench_clock_ns() - t0;
    if (q) p = q;
  }
  bench_free(p);
  r->n = n;
  r->rss_bytes_after = bench_rss_bytes();
}

/* =========================================================================
 * Scenario 7 — fragmentation_churn
 * Allocate a working set of N pointers, randomly replace them.
 * Measure RSS overhead vs theoretical minimum.
 * ====================================================================== */
#define FRAG_SLOTS 65536

void bench_fragmentation_churn(bench_result_t* r, const bench_config_t* cfg) {
  r->rss_bytes_before = bench_rss_bytes();
  void** ptrs = (void**)malloc(FRAG_SLOTS * sizeof(void*));
  memset(ptrs, 0, FRAG_SLOTS * sizeof(void*));
  uint64_t rng = 0x1111222233334444ULL;
  size_t n = cfg->iterations < BENCH_MAX_SAMPLES ? cfg->iterations : BENCH_MAX_SAMPLES;

  /* fill initial set */
  for (int i = 0; i < FRAG_SLOTS; i++) {
    ptrs[i] = bench_malloc(_rand_size(&rng, cfg->size_lo, cfg->size_hi));
  }

  for (size_t i = 0; i < n; i++) {
    size_t idx = (size_t)(_lcg(&rng) % FRAG_SLOTS);
    bench_free(ptrs[idx]);
    size_t sz = _rand_size(&rng, cfg->size_lo, cfg->size_hi);
    uint64_t t0 = bench_clock_ns();
    ptrs[idx] = bench_malloc(sz);
    r->samples[i] = bench_clock_ns() - t0;
  }

  for (int i = 0; i < FRAG_SLOTS; i++) bench_free(ptrs[i]);
  free(ptrs);
  r->n = n;
  r->rss_bytes_after = bench_rss_bytes();
}

/* =========================================================================
 * Scenario 8 — mixed_sizes (simulate real app: small+medium+large)
 * ====================================================================== */
void bench_mixed_sizes(bench_result_t* r, const bench_config_t* cfg) {
  r->rss_bytes_before = bench_rss_bytes();
  size_t n = cfg->iterations < BENCH_MAX_SAMPLES ? cfg->iterations : BENCH_MAX_SAMPLES;
  uint64_t rng = 0x9988776655443322ULL;

  /* size distribution buckets:
     60% small  (8-256 B)
     30% medium (256 B - 64 KiB)
     10% large  (64 KiB - 1 MiB) */
  for (size_t i = 0; i < n; i++) {
    uint64_t roll = _lcg(&rng) % 100;
    size_t sz;
    if (roll < 60)      sz = _rand_size(&rng, 8, 256);
    else if (roll < 90) sz = _rand_size(&rng, 256, 65536);
    else                sz = _rand_size(&rng, 65536, 1024*1024);

    uint64_t t0 = bench_clock_ns();
    void* p = bench_malloc(sz);
    bench_free(p);
    r->samples[i] = bench_clock_ns() - t0;
  }
  r->n = n;
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
  /* for cross-thread: producer fills queue, consumer empties it */
  void**        shared_queue;
  int           queue_size;
  atomic_int*   queue_head;
  atomic_int*   queue_tail;
  atomic_int*   done_flag;
} thread_arg_t;

static void* _mt_alloc_free_worker(void* arg) {
  thread_arg_t* a = (thread_arg_t*)arg;
  uint64_t rng = 0xbabebabe ^ (uint64_t)a->thread_id;
  size_t max_n = a->iterations < BENCH_MAX_SAMPLES ? a->iterations : BENCH_MAX_SAMPLES;

  for (size_t i = 0; i < a->warmup; i++) {
    void* p = bench_malloc(_rand_size(&rng, a->size_lo, a->size_hi));
    bench_free(p);
  }
  for (size_t i = 0; i < max_n; i++) {
    size_t sz = _rand_size(&rng, a->size_lo, a->size_hi);
    uint64_t t0 = bench_clock_ns();
    void* p = bench_malloc(sz);
    bench_free(p);
    a->samples[i] = bench_clock_ns() - t0;
  }
  a->n_samples = max_n;
  return NULL;
}

static void _run_threads(bench_result_t* r, const bench_config_t* cfg,
                          void* (*worker)(void*)) {
  int nt = cfg->nthreads;
  pthread_t* tids         = (pthread_t*)       malloc((size_t)nt * sizeof(pthread_t));
  thread_arg_t* args      = (thread_arg_t*)     malloc((size_t)nt * sizeof(thread_arg_t));
  uint64_t** thread_samples = (uint64_t**)      malloc((size_t)nt * sizeof(uint64_t*));

  r->rss_bytes_before = bench_rss_bytes();

  for (int i = 0; i < nt; i++) {
    thread_samples[i] = (uint64_t*)malloc(BENCH_MAX_SAMPLES * sizeof(uint64_t));
    args[i].thread_id  = i;
    args[i].nthreads   = nt;
    args[i].size_lo    = cfg->size_lo;
    args[i].size_hi    = cfg->size_hi;
    args[i].iterations = cfg->iterations / (size_t)nt;
    args[i].warmup     = cfg->warmup / (size_t)nt;
    args[i].samples    = thread_samples[i];
    args[i].n_samples  = 0;
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
  }
  r->n = total_n;
  r->rss_bytes_after = bench_rss_bytes();

  free(tids); free(args); free(thread_samples);
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
} ring_ctx_t;

static void* _producer(void* arg) {
  ring_ctx_t* ctx = (ring_ctx_t*)arg;
  uint64_t rng = 0xdeadcafe;
  for (size_t i = 0; i < ctx->n_iter; i++) {
    size_t sz = _rand_size(&rng, ctx->size_lo, ctx->size_hi);
    void* p = bench_malloc(sz);
    int tail;
    /* spin-wait if full */
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
    bench_free(p);
    if (freed < max_s) ctx->samples[freed] = bench_clock_ns() - t0;
    freed++;
  }
  ctx->n_samples = freed < max_s ? freed : max_s;
  return NULL;
}

void bench_cross_thread(bench_result_t* r, const bench_config_t* cfg) {
  r->rss_bytes_before = bench_rss_bytes();
  ring_ctx_t ctx = {0};
  ctx.ring   = (void**)malloc(RING_SIZE * sizeof(void*));
  memset(ctx.ring, 0, RING_SIZE * sizeof(void*));
  ctx.size_lo = cfg->size_lo;
  ctx.size_hi = cfg->size_hi;
  ctx.n_iter  = cfg->iterations < 500000 ? cfg->iterations : 500000;
  ctx.samples = r->samples;
  atomic_init(&ctx.head, 0);
  atomic_init(&ctx.tail, 0);
  atomic_init(&ctx.done, 0);

  pthread_t prod_tid, cons_tid;
  pthread_create(&prod_tid, NULL, _producer, &ctx);
  pthread_create(&cons_tid, NULL, _consumer, &ctx);
  pthread_join(prod_tid, NULL);
  pthread_join(cons_tid, NULL);

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
 * Scenario — object_pool (fixed small size, high thread contention)
 * ====================================================================== */
static void* _pool_worker(void* arg) {
  thread_arg_t* a = (thread_arg_t*)arg;
  (void)a->thread_id; /* suppress unused if needed */
  size_t sz = 64; /* fixed small pool slot */
  size_t n = a->iterations;
  if (n > BENCH_MAX_SAMPLES) n = BENCH_MAX_SAMPLES;

  for (size_t i = 0; i < a->warmup; i++) {
    void* p = bench_malloc(sz); bench_free(p);
  }
  for (size_t i = 0; i < n; i++) {
    uint64_t t0 = bench_clock_ns();
    void* p = bench_malloc(sz);
    bench_free(p);
    a->samples[i] = bench_clock_ns() - t0;
  }
  a->n_samples = n;
  return NULL;
}

void bench_object_pool(bench_result_t* r, const bench_config_t* cfg) {
  _run_threads(r, cfg, _pool_worker);
}

/* =========================================================================
 * Scenario — peak_rss
 * Measures maximum RSS during a large working set
 * ====================================================================== */
void bench_peak_rss(bench_result_t* r, const bench_config_t* cfg) {
  const size_t N = 65536;
  void** ptrs = (void**)malloc(N * sizeof(void*));
  memset(ptrs, 0, N * sizeof(void*));
  uint64_t rng = 0xdeadbeef;
  r->rss_bytes_before = bench_rss_bytes();

  for (size_t i = 0; i < N; i++) {
    size_t sz = _rand_size(&rng, cfg->size_lo, cfg->size_hi);
    ptrs[i] = bench_malloc(sz);
    if (ptrs[i]) _touch((volatile uint8_t*)ptrs[i], sz);
  }
  long rss_peak = bench_rss_bytes();

  uint64_t t0 = bench_clock_ns();
  for (size_t i = 0; i < N; i++) bench_free(ptrs[i]);
  r->samples[0] = bench_clock_ns() - t0;
  free(ptrs);

  r->n = 1;
  r->rss_bytes_after = rss_peak;
}

/* =========================================================================
 * Scenario table (exported)
 * ====================================================================== */
bench_scenario_entry_t g_scenarios[] = {
  { "alloc_free_same_thread",  bench_alloc_free_same_thread, "Single-thread alloc+free (small)",         8,    256   },
  { "alloc_free_batch",        bench_alloc_free_batch,        "Batch alloc then free (small)",            8,    256   },
  { "latency_small",           bench_latency_small,           "Alloc latency p50/p99 (8-256B)",           8,    256   },
  { "latency_large",           bench_latency_large,           "Alloc latency p50/p99 (1 MiB)",            1<<20,1<<20 },
  { "calloc_bench",            bench_calloc_scenario,         "Calloc throughput (zeroed, 256B-64KiB)",   256,  65536 },
  { "realloc_bench",           bench_realloc_scenario,        "Realloc throughput (16B-64KiB)",           16,   65536 },
  { "fragmentation_churn",     bench_fragmentation_churn,     "Fragmentation under churn (8-512B)",       8,    512   },
  { "mixed_sizes",             bench_mixed_sizes,             "Mixed size distribution (realistic)",      8,    1<<20 },
  { "alloc_free_mt",           bench_alloc_free_mt,           "Multi-thread alloc+free (small)",          8,    256   },
  { "cross_thread",            bench_cross_thread,            "Cross-thread ownership transfer",          8,    256   },
  { "thread_scale",            bench_thread_scale,            "Thread scaling 1->32 (p50 latency)",       8,    256   },
  { "object_pool",             bench_object_pool,             "Fixed-size pool contention (64B)",         64,   64    },
  { "peak_rss",                bench_peak_rss,                "Peak RSS during large working set",        256,  65536 },
  { NULL, NULL, NULL, 0, 0 }
};

const char* bench_alloc_name(void) { return ALLOC_NAME; }
