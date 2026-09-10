/* adversarial_bench.c — Implementation of adversarial vector benchmarks
 * ============================================================================
 */
#include "adversarial_bench.h"
#include <errno.h>

/* -------------------------------------------------------------------------
 * Allocator Implementations
 * ------------------------------------------------------------------------- */

/* System allocator (glibc ptmalloc) */
static void* _sys_malloc_aligned(size_t size, size_t alignment) {
#ifdef __linux__
    void* ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) return NULL;
    return ptr;
#else
    /* Fallback for non-Linux systems */
    size_t aligned_size = size + alignment - 1;
    void* raw = malloc(aligned_size);
    if (!raw) return NULL;
    uintptr_t ptr = ((uintptr_t)raw + alignment - 1) & ~(alignment - 1);
    return (void*)ptr;
#endif
}

static void _sys_free(void* ptr) {
    free(ptr);
}

static void* _sys_arena_create(size_t capacity) {
    /* System allocator has no native arena - use malloc for backing */
    return malloc(capacity);
}

static void* _sys_arena_alloc(void* arena, size_t size, size_t alignment) {
    /* Simple bump allocator over malloc'd backing */
    return _sys_malloc_aligned(size, alignment);
}

static void _sys_arena_reset(void* arena) {
    /* No-op for system allocator - individual frees required */
    (void)arena;
}

static void _sys_arena_destroy(void* arena) {
    free(arena);
}

adv_allocator_t adv_system_allocator = {
    .malloc_aligned = _sys_malloc_aligned,
    .free = _sys_free,
    .arena_create = _sys_arena_create,
    .arena_alloc = _sys_arena_alloc,
    .arena_reset = _sys_arena_reset,
    .arena_destroy = _sys_arena_destroy,
    .name = "system"
};

/* Palloc allocator */
#ifdef BENCH_USE_PALLOC
#include <palloc.h>
#include "palloc/arena_pomai.h"

static void* _palloc_malloc_aligned(size_t size, size_t alignment) {
    return pa_malloc_aligned(size, alignment);
}

static void _palloc_free(void* ptr) {
    pa_free(ptr);
}

static void* _palloc_arena_create(size_t capacity) {
    return p_arena_create_for_vector(capacity);
}

static void* _palloc_arena_alloc(void* arena, size_t size, size_t alignment) {
    /* Palloc arena handles alignment internally */
    return p_arena_alloc_vector((pa_arena_t*)arena, size / sizeof(float), sizeof(float));
}

static void _palloc_arena_reset(void* arena) {
    p_arena_reset((pa_arena_t*)arena);
}

static void _palloc_arena_destroy(void* arena) {
    p_arena_destroy((pa_arena_t*)arena);
}

adv_allocator_t adv_palloc_allocator = {
    .malloc_aligned = _palloc_malloc_aligned,
    .free = _palloc_free,
    .arena_create = _palloc_arena_create,
    .arena_alloc = _palloc_arena_alloc,
    .arena_reset = _palloc_arena_reset,
    .arena_destroy = _palloc_arena_destroy,
    .name = "palloc"
};
#else
adv_allocator_t adv_palloc_allocator = { .name = "palloc (not built)" };
#endif

/* Mimalloc allocator */
#ifdef BENCH_USE_MIMALLOC
#include <mimalloc.h>

static void* _mi_malloc_aligned(size_t size, size_t alignment) {
    return mi_malloc_aligned(size, alignment);
}

static void _mi_free(void* ptr) {
    mi_free(ptr);
}

adv_allocator_t adv_mimalloc_allocator = {
    .malloc_aligned = _mi_malloc_aligned,
    .free = _mi_free,
    .arena_create = _sys_arena_create,  /* No native arena */
    .arena_alloc = _sys_arena_alloc,
    .arena_reset = _sys_arena_reset,
    .arena_destroy = _sys_arena_destroy,
    .name = "mimalloc"
};
#else
adv_allocator_t adv_mimalloc_allocator = { .name = "mimalloc (not built)" };
#endif

/* Jemalloc allocator */
#ifdef BENCH_USE_JEMALLOC
static void* _je_malloc_aligned(size_t size, size_t alignment) {
    void* ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) return NULL;
    return ptr;
}

static void _je_free(void* ptr) {
    free(ptr);  /* jemalloc hooks into malloc/free */
}

adv_allocator_t adv_jemalloc_allocator = {
    .malloc_aligned = _je_malloc_aligned,
    .free = _je_free,
    .arena_create = _sys_arena_create,
    .arena_alloc = _sys_arena_alloc,
    .arena_reset = _sys_arena_reset,
    .arena_destroy = _sys_arena_destroy,
    .name = "jemalloc"
};
#else
adv_allocator_t adv_jemalloc_allocator = { .name = "jemalloc (not built)" };
#endif

/* Tcmalloc allocator */
#ifdef BENCH_USE_TCMALLOC
static void* _tc_malloc_aligned(size_t size, size_t alignment) {
    void* ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) return NULL;
    return ptr;
}

static void _tc_free(void* ptr) {
    free(ptr);  /* tcmalloc hooks into malloc/free */
}

adv_allocator_t adv_tcmalloc_allocator = {
    .malloc_aligned = _tc_malloc_aligned,
    .free = _tc_free,
    .arena_create = _sys_arena_create,
    .arena_alloc = _sys_arena_alloc,
    .arena_reset = _sys_arena_reset,
    .arena_destroy = _sys_arena_destroy,
    .name = "tcmalloc"
};
#else
adv_allocator_t adv_tcmalloc_allocator = { .name = "tcmalloc (not built)" };
#endif

/* -------------------------------------------------------------------------
 * Scenario 1: Vector Batch Churn
 * ========================================================================= */
void adv_batch_churn_benchmark(adv_result_t* result,
                                const adv_allocator_t* alloc,
                                const adv_batch_churn_config_t* config) {
    memset(result, 0, sizeof(adv_result_t));
    result->alloc_name = alloc->name;
    result->scenario = "batch_churn";
    
    /* Generate variant name */
    char variant[64];
    snprintf(variant, sizeof(variant), "batch-%zu-dim-%zu", 
             config->batch_size, config->vector_dim);
    result->variant = strdup(variant);
    
    result->rss_before = adv_rss_bytes();
    
    const size_t vector_bytes = config->vector_dim * ADV_VECTOR_ELEMSZ;
    const size_t aligned_vector_bytes = adv_aligned_size(vector_bytes, ADV_VECTOR_ALIGN);
    result->requested_bytes = config->batch_size * vector_bytes;
    
    void** pointers = malloc(config->batch_size * sizeof(void*));
    if (!pointers) {
        result->n_samples = 0;
        return;
    }
    
    void* arena = NULL;
    if (config->use_arena && alloc->arena_create) {
        size_t arena_capacity = config->batch_size * aligned_vector_bytes * 2;
        arena = alloc->arena_create(arena_capacity);
    }
    
    const size_t n_iterations = config->n_iterations;
    const size_t n_samples = (n_iterations < ADV_MAX_SAMPLES) ? n_iterations : ADV_MAX_SAMPLES;
    result->samples = calloc(n_samples, sizeof(uint64_t));
    
    /* Warmup */
    for (int i = 0; i < 100; i++) {
        if (arena) {
            alloc->arena_reset(arena);
            for (size_t j = 0; j < config->batch_size; j++) {
                void* ptr = alloc->arena_alloc(arena, vector_bytes, ADV_VECTOR_ALIGN);
                if (!ptr) break;
            }
        } else {
            for (size_t j = 0; j < config->batch_size; j++) {
                void* ptr = alloc->malloc_aligned(vector_bytes, ADV_VECTOR_ALIGN);
                if (!ptr) break;
                alloc->free(ptr);
            }
        }
    }
    
    /* Main benchmark */
    size_t sample_idx = 0;
    size_t allocated = 0;  /* Move outside loop for later use */
    for (size_t iter = 0; iter < n_iterations && sample_idx < n_samples; iter++) {
        uint64_t t0 = adv_clock_ns();
        
        /* Batch allocation */
        allocated = 0;
        if (arena) {
            alloc->arena_reset(arena);
            for (size_t j = 0; j < config->batch_size; j++) {
                pointers[j] = alloc->arena_alloc(arena, vector_bytes, ADV_VECTOR_ALIGN);
                if (!pointers[j]) break;
                allocated++;
            }
        } else {
            for (size_t j = 0; j < config->batch_size; j++) {
                pointers[j] = alloc->malloc_aligned(vector_bytes, ADV_VECTOR_ALIGN);
                if (!pointers[j]) break;
                allocated++;
            }
        }
        
        /* Batch deallocation */
        if (arena) {
            alloc->arena_reset(arena);
        } else {
            for (size_t j = 0; j < allocated; j++) {
                alloc->free(pointers[j]);
            }
        }
        
        uint64_t t1 = adv_clock_ns();
        result->samples[sample_idx++] = (allocated > 0) ? (t1 - t0) / allocated : 0;
    }
    
    result->n_samples = sample_idx;
    result->rss_after = adv_rss_bytes();
    result->rss_peak = result->rss_after;
    
    /* Estimate allocated bytes (with padding) */
    result->allocated_bytes = allocated * aligned_vector_bytes;
    
    if (arena) {
        alloc->arena_destroy(arena);
    }
    
    free(pointers);
    adv_stats_compute(result);
}

/* -------------------------------------------------------------------------
 * Scenario 2: SIMD Cacheline-Split & Access Latency
 * ========================================================================= */
void adv_simd_latency_benchmark(adv_result_t* result,
                                const adv_allocator_t* alloc,
                                const adv_simd_latency_config_t* config) {
    memset(result, 0, sizeof(adv_result_t));
    result->alloc_name = alloc->name;
    result->scenario = "simd_latency";
    
    char variant[64];
    snprintf(variant, sizeof(variant), "dim-%zu-%s-%s", 
             config->vector_dim,
             config->cold_cache ? "cold" : "hot",
             config->unaligned_access ? "unaligned" : "aligned");
    result->variant = strdup(variant);
    
    result->rss_before = adv_rss_bytes();
    
    const size_t vector_bytes = config->vector_dim * ADV_VECTOR_ELEMSZ;
    result->requested_bytes = vector_bytes;
    
    const size_t n_iterations = config->n_iterations;
    const size_t n_samples = (n_iterations < ADV_MAX_SAMPLES) ? n_iterations : ADV_MAX_SAMPLES;
    result->samples = calloc(n_samples, sizeof(uint64_t));
    
    /* Cold cache setup: allocate large buffer to flush cache */
    void* cache_flush_buffer = NULL;
    if (config->cold_cache) {
        const size_t flush_size = 256 * 1024 * 1024; /* 256 MB */
        cache_flush_buffer = malloc(flush_size);
        if (cache_flush_buffer) {
            adv_touch_pages((volatile uint8_t*)cache_flush_buffer, flush_size);
        }
    }
    
    /* Unaligned access setup: force offset */
    const size_t alloc_size = config->unaligned_access ? 
                              vector_bytes + ADV_VECTOR_ALIGN - 1 : 
                              vector_bytes;
    
    /* Warmup */
    for (int i = 0; i < 50; i++) {
        void* ptr = alloc->malloc_aligned(alloc_size, ADV_VECTOR_ALIGN);
        if (!ptr) break;
        
        float* float_ptr = config->unaligned_access ? 
                           (float*)((uint8_t*)ptr + 1) : /* Force unaligned */
                           (float*)ptr;
        
#ifdef __AVX512F__
        adv_simd_touch_avx512(float_ptr, config->vector_dim);
#elif defined(__AVX2__)
        adv_simd_touch_avx2(float_ptr, config->vector_dim);
#else
        adv_simd_touch_scalar(float_ptr, config->vector_dim);
#endif
        
        alloc->free(ptr);
    }
    
    /* Main benchmark */
    size_t sample_idx = 0;
    for (size_t iter = 0; iter < n_iterations && sample_idx < n_samples; iter++) {
        /* Flush cache if cold cache mode */
        if (config->cold_cache && cache_flush_buffer) {
            const size_t flush_size = 256 * 1024 * 1024;
            adv_touch_pages((volatile uint8_t*)cache_flush_buffer, flush_size);
        }
        
        uint64_t t0 = adv_clock_ns();
        void* ptr = alloc->malloc_aligned(alloc_size, ADV_VECTOR_ALIGN);
        if (!ptr) break;
        
        float* float_ptr = config->unaligned_access ? 
                           (float*)((uint8_t*)ptr + 1) : 
                           (float*)ptr;
        
#ifdef __AVX512F__
        adv_simd_touch_avx512(float_ptr, config->vector_dim);
#elif defined(__AVX2__)
        adv_simd_touch_avx2(float_ptr, config->vector_dim);
#else
        adv_simd_touch_scalar(float_ptr, config->vector_dim);
#endif
        
        uint64_t t1 = adv_clock_ns();
        alloc->free(ptr);
        
        result->samples[sample_idx++] = t1 - t0;
    }
    
    result->n_samples = sample_idx;
    result->rss_after = adv_rss_bytes();
    result->rss_peak = result->rss_after;
    
    /* Estimate allocated bytes */
    result->allocated_bytes = adv_aligned_size(alloc_size, ADV_VECTOR_ALIGN);
    
    if (cache_flush_buffer) {
        free(cache_flush_buffer);
    }
    
    adv_stats_compute(result);
}

/* -------------------------------------------------------------------------
 * Scenario 3: Large Working-Set TLB Pressure
 * ========================================================================= */
void adv_tlb_pressure_benchmark(adv_result_t* result,
                                const adv_allocator_t* alloc,
                                const adv_tlb_pressure_config_t* config) {
    memset(result, 0, sizeof(adv_result_t));
    result->alloc_name = alloc->name;
    result->scenario = "tlb_pressure";
    
    char variant[64];
    snprintf(variant, sizeof(variant), "ws-%zuGB-dim-%zu-stride-%zuMB-%s", 
             config->working_set_gb,
             config->vector_dim,
             config->stride_mb,
             config->use_thp ? "thp" : "4kb");
    result->variant = strdup(variant);
    
    result->rss_before = adv_rss_bytes();
    
    const size_t vector_bytes = config->vector_dim * ADV_VECTOR_ELEMSZ;
    const size_t stride_bytes = config->stride_mb * 1024 * 1024;
    const size_t working_set_bytes = config->working_set_gb * 1024 * 1024 * 1024;
    
    const size_t n_vectors = working_set_bytes / stride_bytes;
    if (n_vectors == 0) {
        result->n_samples = 0;
        return;
    }
    
    result->requested_bytes = n_vectors * vector_bytes;
    
    void** pointers = calloc(n_vectors, sizeof(void*));
    if (!pointers) {
        result->n_samples = 0;
        return;
    }
    
    /* Enable THP if requested */
    if (config->use_thp) {
#ifdef __linux__
        madvise(NULL, 0, MADV_HUGEPAGE);
#endif
    }
    
    /* Allocate large working set */
    size_t allocated = 0;
    for (size_t i = 0; i < n_vectors; i++) {
        void* ptr = alloc->malloc_aligned(vector_bytes, ADV_VECTOR_ALIGN);
        if (!ptr) break;
        pointers[i] = ptr;
        adv_touch_pages((volatile uint8_t*)ptr, vector_bytes);
        allocated++;
    }
    
    result->rss_after = adv_rss_bytes();
    result->rss_peak = result->rss_after;
    result->allocated_bytes = allocated * adv_aligned_size(vector_bytes, ADV_VECTOR_ALIGN);
    
    /* Benchmark: sequential access with stride pattern */
    const size_t n_iterations = 1000;
    const size_t n_samples = (n_iterations < ADV_MAX_SAMPLES) ? n_iterations : ADV_MAX_SAMPLES;
    result->samples = calloc(n_samples, sizeof(uint64_t));
    
    size_t sample_idx = 0;
    for (size_t iter = 0; iter < n_iterations && sample_idx < n_samples; iter++) {
        uint64_t t0 = adv_clock_ns();
        
        /* Touch each vector with stride pattern */
        for (size_t i = 0; i < allocated; i++) {
            float* ptr = (float*)pointers[i];
#ifdef __AVX512F__
            adv_simd_touch_avx512(ptr, config->vector_dim);
#elif defined(__AVX2__)
            adv_simd_touch_avx2(ptr, config->vector_dim);
#else
            adv_simd_touch_scalar(ptr, config->vector_dim);
#endif
        }
        
        uint64_t t1 = adv_clock_ns();
        result->samples[sample_idx++] = (allocated > 0) ? (t1 - t0) / allocated : 0;
    }
    
    result->n_samples = sample_idx;
    
    /* Cleanup */
    for (size_t i = 0; i < allocated; i++) {
        alloc->free(pointers[i]);
    }
    free(pointers);
    
    adv_stats_compute(result);
}

/* -------------------------------------------------------------------------
 * Output Functions
 * ========================================================================= */
void adv_print_csv_header(void) {
    printf("allocator,scenario,variant,ops_per_sec,min_ns,p50_ns,p90_ns,p99_ns,max_ns,mean_ns,stddev_ns,rss_before,rss_after,rss_peak,requested_bytes,allocated_bytes,fragmentation_ratio\n");
}

void adv_print_table_header(void) {
    printf("%-16s %-20s %-25s %12s %8s %8s %8s %8s %8s %12s %12s\n",
           "Allocator", "Scenario", "Variant", "Throughput", 
           "min ns", "p50 ns", "p90 ns", "p99 ns", "max ns", 
           "RSS", "Frag Ratio");
    printf("%-16s %-20s %-25s %12s %8s %8s %8s %8s %8s %12s %12s\n",
           "----------------", "--------------------", "-------------------------",
           "------------", "--------", "--------", "--------", "--------", "--------",
           "------------", "------------");
}

void adv_print_markdown_header(void) {
    printf("| %-16s | %-20s | %-25s | %12s | %8s | %8s | %8s | %8s | %8s | %12s | %12s |\n",
           "Allocator", "Scenario", "Variant", "Throughput",
           "min ns", "p50 ns", "p90 ns", "p99 ns", "max ns",
           "RSS", "Frag Ratio");
    printf("|-%s-|-%s-|-%s-|-%s:|-%s:|-%s:|-%s:|-%s:|-%s:|-%s:|-%s:|\n",
           "----------------", "--------------------", "-------------------------",
           "------------", "--------", "--------", "--------", "--------", "--------",
           "------------", "------------");
}

void adv_print_result(const adv_result_t* result, adv_output_fmt_t fmt) {
    char ops_buf[32], rss_buf[32], frag_buf[32];
    
    if (result->ops_per_sec >= 1e9) {
        snprintf(ops_buf, sizeof(ops_buf), "%.2f Gop/s", result->ops_per_sec / 1e9);
    } else if (result->ops_per_sec >= 1e6) {
        snprintf(ops_buf, sizeof(ops_buf), "%.2f Mop/s", result->ops_per_sec / 1e6);
    } else if (result->ops_per_sec >= 1e3) {
        snprintf(ops_buf, sizeof(ops_buf), "%.2f Kop/s", result->ops_per_sec / 1e3);
    } else {
        snprintf(ops_buf, sizeof(ops_buf), "%.2f op/s", result->ops_per_sec);
    }
    
    if (result->rss_after >= 1024 * 1024 * 1024) {
        snprintf(rss_buf, sizeof(rss_buf), "%.1f GiB", (double)result->rss_after / (1024. * 1024 * 1024));
    } else if (result->rss_after >= 1024 * 1024) {
        snprintf(rss_buf, sizeof(rss_buf), "%.1f MiB", (double)result->rss_after / (1024. * 1024));
    } else if (result->rss_after >= 1024) {
        snprintf(rss_buf, sizeof(rss_buf), "%.1f KiB", (double)result->rss_after / 1024.);
    } else {
        snprintf(rss_buf, sizeof(rss_buf), "%ld B", result->rss_after);
    }
    
    snprintf(frag_buf, sizeof(frag_buf), "%.3f", result->fragmentation_ratio);
    
    switch (fmt) {
    case ADV_OUTPUT_CSV:
        printf("%s,%s,%s,%.2f,%llu,%llu,%llu,%llu,%llu,%.2f,%.2f,%ld,%ld,%ld,%zu,%zu,%.3f\n",
               result->alloc_name, result->scenario, result->variant,
               result->ops_per_sec,
               (unsigned long long)result->min_ns,
               (unsigned long long)result->p50_ns,
               (unsigned long long)result->p90_ns,
               (unsigned long long)result->p99_ns,
               (unsigned long long)result->max_ns,
               result->mean_ns, result->stddev_ns,
               result->rss_before, result->rss_after, result->rss_peak,
               result->requested_bytes, result->allocated_bytes,
               result->fragmentation_ratio);
        break;
        
    case ADV_OUTPUT_JSON:
        printf("{\"allocator\":\"%s\",\"scenario\":\"%s\",\"variant\":\"%s\","
               "\"ops_per_sec\":%.2f,"
               "\"min_ns\":%llu,\"p50_ns\":%llu,\"p90_ns\":%llu,\"p99_ns\":%llu,\"max_ns\":%llu,"
               "\"mean_ns\":%.2f,\"stddev_ns\":%.2f,"
               "\"rss_before\":%ld,\"rss_after\":%ld,\"rss_peak\":%ld,"
               "\"requested_bytes\":%zu,\"allocated_bytes\":%zu,\"fragmentation_ratio\":%.3f},\n",
               result->alloc_name, result->scenario, result->variant,
               result->ops_per_sec,
               (unsigned long long)result->min_ns,
               (unsigned long long)result->p50_ns,
               (unsigned long long)result->p90_ns,
               (unsigned long long)result->p99_ns,
               (unsigned long long)result->max_ns,
               result->mean_ns, result->stddev_ns,
               result->rss_before, result->rss_after, result->rss_peak,
               result->requested_bytes, result->allocated_bytes,
               result->fragmentation_ratio);
        break;
        
    case ADV_OUTPUT_MARKDOWN:
        printf("| %-16s | %-20s | %-25s | %12s | %8llu | %8llu | %8llu | %8llu | %8llu | %12s | %12s |\n",
               result->alloc_name, result->scenario, result->variant, ops_buf,
               (unsigned long long)result->min_ns,
               (unsigned long long)result->p50_ns,
               (unsigned long long)result->p90_ns,
               (unsigned long long)result->p99_ns,
               (unsigned long long)result->max_ns,
               rss_buf, frag_buf);
        break;
        
    default: /* TABLE */
        printf("%-16s %-20s %-25s %12s %8llu %8llu %8llu %8llu %8llu %12s %12s\n",
               result->alloc_name, result->scenario, result->variant, ops_buf,
               (unsigned long long)result->min_ns,
               (unsigned long long)result->p50_ns,
               (unsigned long long)result->p90_ns,
               (unsigned long long)result->p99_ns,
               (unsigned long long)result->max_ns,
               rss_buf, frag_buf);
        break;
    }
}