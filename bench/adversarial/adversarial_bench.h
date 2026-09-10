/* adversarial_bench.h — Adversarial vector benchmarks for allocator evaluation
 * ============================================================================
 * Publication-grade benchmark harness designed to expose architectural 
 * limitations of general-purpose allocators when subjected to SIMD-aligned,
 * batch-heavy vector workloads, highlighting palloc's core strengths.
 *
 * Targeted stress tests:
 * 1. Vector Batch Churn (Bulk Allocation & Free)
 * 2. SIMD Cacheline-Split & Access Latency
 * 3. Large Working-Set TLB Pressure
 */
#ifndef ADVERSARIAL_BENCH_H
#define ADVERSARIAL_BENCH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>

#ifdef __linux__
#include <sys/resource.h>
#include <sys/mman.h>
#endif

/* -------------------------------------------------------------------------
 * Configuration
 * ------------------------------------------------------------------------- */
#define ADV_MAX_SAMPLES 1048576
#define ADV_VECTOR_ALIGN 64  /* AVX-512 cacheline alignment */
#define ADV_VECTOR_ELEM float
#define ADV_VECTOR_ELEMSZ sizeof(ADV_VECTOR_ELEM)

/* Standard embedding dimensions (in floats) */
#define ADV_DIM_384   384   /* ~1.5 KB */
#define ADV_DIM_768   768   /* ~3 KB */
#define ADV_DIM_1536  1536  /* ~6 KB */
#define ADV_DIM_4096  4096  /* ~16 KB */

/* Batch sizes for churn testing */
#define ADV_BATCH_32    32
#define ADV_BATCH_128   128
#define ADV_BATCH_512   512
#define ADV_BATCH_4096  4096

/* -------------------------------------------------------------------------
 * Timing and Memory Metrics
 * ------------------------------------------------------------------------- */
static inline uint64_t adv_clock_ns(void) {
    struct timespec ts;
#ifdef CLOCK_MONOTONIC_RAW
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static inline long adv_rss_bytes(void) {
#ifdef __linux__
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_maxrss * 1024L;  /* Linux: kilobytes -> bytes */
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------
 * Benchmark Result Structure
 * ------------------------------------------------------------------------- */
typedef struct {
    const char* alloc_name;
    const char* scenario;
    const char* variant;        /* e.g., "batch-128", "dim-768" */
    
    uint64_t* samples;
    size_t n_samples;
    
    /* Performance metrics */
    double ops_per_sec;
    uint64_t min_ns;
    uint64_t p50_ns;
    uint64_t p90_ns;
    uint64_t p99_ns;
    uint64_t max_ns;
    double mean_ns;
    double stddev_ns;
    
    /* Memory metrics */
    long rss_before;
    long rss_after;
    long rss_peak;
    size_t requested_bytes;
    size_t allocated_bytes;
    double fragmentation_ratio;  /* (allocated - requested) / requested */
    
    /* PMU counters (if available) */
    uint64_t cpu_cycles;
    uint64_t instructions;
    uint64_t l1d_cache_misses;
    uint64_t llc_cache_misses;
    uint64_t dtlb_load_misses;
    uint64_t page_faults;
} adv_result_t;

/* -------------------------------------------------------------------------
 * Statistics Computation
 * ------------------------------------------------------------------------- */
static int _adv_u64_cmp(const void* a, const void* b) {
    uint64_t x = *(const uint64_t*)a;
    uint64_t y = *(const uint64_t*)b;
    return (x > y) - (x < y);
}

static inline void adv_stats_compute(adv_result_t* r) {
    if (r->n_samples == 0) return;
    
    qsort(r->samples, r->n_samples, sizeof(uint64_t), _adv_u64_cmp);
    r->min_ns = r->samples[0];
    r->max_ns = r->samples[r->n_samples - 1];
    r->p50_ns = r->samples[(size_t)(r->n_samples * 0.50)];
    r->p90_ns = r->samples[(size_t)(r->n_samples * 0.90)];
    r->p99_ns = r->samples[(size_t)(r->n_samples * 0.99)];
    
    double sum = 0;
    for (size_t i = 0; i < r->n_samples; i++) sum += (double)r->samples[i];
    r->mean_ns = sum / (double)r->n_samples;
    
    double var = 0;
    for (size_t i = 0; i < r->n_samples; i++) {
        double d = (double)r->samples[i] - r->mean_ns;
        var += d * d;
    }
    r->stddev_ns = sqrt(var / (double)r->n_samples);
    
    double total_ns = sum;
    r->ops_per_sec = (total_ns > 0) ? ((double)r->n_samples * 1e9 / total_ns) : 0.0;
    
    if (r->requested_bytes > 0) {
        r->fragmentation_ratio = (double)(r->allocated_bytes - r->requested_bytes) / (double)r->requested_bytes;
    }
}

/* -------------------------------------------------------------------------
 * Allocator Dispatch Interface
 * ------------------------------------------------------------------------- */
typedef struct {
    void* (*malloc_aligned)(size_t size, size_t alignment);
    void  (*free)(void* ptr);
    void* (*arena_create)(size_t capacity);
    void* (*arena_alloc)(void* arena, size_t size, size_t alignment);
    void  (*arena_reset)(void* arena);
    void  (*arena_destroy)(void* arena);
    const char* name;
} adv_allocator_t;

/* -------------------------------------------------------------------------
 * SIMD Touch Operations
 * ------------------------------------------------------------------------- */
#if defined(__AVX2__)
#include <immintrin.h>
static inline void adv_simd_touch_avx2(float* ptr, size_t n_floats) {
    /* Process 8 floats per iteration (256-bit AVX2) */
    size_t i = 0;
    __m256 sum = _mm256_setzero_ps();
    for (; i + 8 <= n_floats; i += 8) {
        __m256 v = _mm256_load_ps(ptr + i);
        sum = _mm256_add_ps(sum, v);
    }
    /* Prevent compiler from optimizing away */
    volatile float result = _mm256_cvtss_f32(_mm256_hadd_ps(sum, sum));
    (void)result;
}
#elif defined(__AVX512F__)
#include <immintrin.h>
static inline void adv_simd_touch_avx512(float* ptr, size_t n_floats) {
    /* Process 16 floats per iteration (512-bit AVX-512) */
    size_t i = 0;
    __m512 sum = _mm512_setzero_ps();
    for (; i + 16 <= n_floats; i += 16) {
        __m512 v = _mm512_load_ps(ptr + i);
        sum = _mm512_add_ps(sum, v);
    }
    volatile float result = _mm512_cvtss_f32(_mm512_reduce_add_ps(sum));
    (void)result;
}
#else
static inline void adv_simd_touch_scalar(float* ptr, size_t n_floats) {
    /* Fallback: scalar touch */
    volatile float sum = 0.0f;
    for (size_t i = 0; i < n_floats; i++) {
        sum += ptr[i];
    }
    (void)sum;
}
#endif

/* -------------------------------------------------------------------------
 * Benchmark Scenarios
 * ------------------------------------------------------------------------- */

/* Scenario 1: Vector Batch Churn - Bulk Allocation & Free */
typedef struct {
    size_t batch_size;      /* K vectors per batch */
    size_t vector_dim;      /* Embedding dimension */
    size_t n_iterations;    /* Number of batch cycles */
    bool use_arena;         /* Use arena if available */
} adv_batch_churn_config_t;

void adv_batch_churn_benchmark(adv_result_t* result, 
                                const adv_allocator_t* alloc,
                                const adv_batch_churn_config_t* config);

/* Scenario 2: SIMD Cacheline-Split & Access Latency */
typedef struct {
    size_t vector_dim;
    size_t n_iterations;
    bool cold_cache;        /* Force cold cache by touching large buffer */
    bool unaligned_access;  /* Force unaligned access patterns */
} adv_simd_latency_config_t;

void adv_simd_latency_benchmark(adv_result_t* result,
                                const adv_allocator_t* alloc,
                                const adv_simd_latency_config_t* config);

/* Scenario 3: Large Working-Set TLB Pressure */
typedef struct {
    size_t working_set_gb;  /* Working set size in GB */
    size_t vector_dim;
    size_t stride_mb;       /* Stride between allocations */
    bool use_thp;          /* Attempt to use Transparent Huge Pages */
} adv_tlb_pressure_config_t;

void adv_tlb_pressure_benchmark(adv_result_t* result,
                                const adv_allocator_t* alloc,
                                const adv_tlb_pressure_config_t* config);

/* -------------------------------------------------------------------------
 * Output Functions
 * ------------------------------------------------------------------------- */
typedef enum {
    ADV_OUTPUT_TABLE,
    ADV_OUTPUT_CSV,
    ADV_OUTPUT_JSON,
    ADV_OUTPUT_MARKDOWN
} adv_output_fmt_t;

void adv_print_result(const adv_result_t* result, adv_output_fmt_t fmt);
void adv_print_csv_header(void);
void adv_print_table_header(void);
void adv_print_markdown_header(void);

/* -------------------------------------------------------------------------
 * Utility Functions
 * ------------------------------------------------------------------------- */
static inline void adv_touch_pages(volatile uint8_t* ptr, size_t size) {
    /* Touch every page to force RSS allocation */
    for (size_t i = 0; i < size; i += 4096) {
        ptr[i] = (uint8_t)i;
    }
}

static inline size_t adv_aligned_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

/* -------------------------------------------------------------------------
 * Allocator Implementations
 * ------------------------------------------------------------------------- */
extern adv_allocator_t adv_palloc_allocator;
extern adv_allocator_t adv_system_allocator;
extern adv_allocator_t adv_mimalloc_allocator;
extern adv_allocator_t adv_jemalloc_allocator;
extern adv_allocator_t adv_tcmalloc_allocator;

#endif /* ADVERSARIAL_BENCH_H */