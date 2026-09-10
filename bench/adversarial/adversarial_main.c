/* adversarial_main.c — Main driver for adversarial vector benchmarks
 * ============================================================================
 */
#include "adversarial_bench.h"
#include <getopt.h>
#include <signal.h>
#include <unistd.h>

static volatile bool g_running = true;

static void signal_handler(int sig) {
    (void)sig;
    g_running = false;
}

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("\nAdversarial Vector Benchmark Suite for Allocator Evaluation\n");
    printf("\nOptions:\n");
    printf("  --alloc=NAME       Allocator to test (palloc, system, mimalloc, jemalloc, tcmalloc, all)\n");
    printf("  --scenario=NAME    Scenario to run (batch_churn, simd_latency, tlb_pressure, all)\n");
    printf("  --output=FMT       Output format (table, csv, json, markdown)\n");
    printf("  --iterations=N     Number of iterations per benchmark (default: 10000)\n");
    printf("  --threads=N       Number of threads (default: 1)\n");
    printf("  --help             Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s --alloc=palloc --scenario=all --output=markdown\n", prog);
    printf("  %s --alloc=all --scenario=batch_churn --iterations=50000\n", prog);
}

static void run_batch_churn_suite(const adv_allocator_t* alloc, size_t iterations) {
    const size_t batch_sizes[] = {ADV_BATCH_32, ADV_BATCH_128, ADV_BATCH_512, ADV_BATCH_4096};
    const size_t dims[] = {ADV_DIM_384, ADV_DIM_768, ADV_DIM_1536, ADV_DIM_4096};
    
    for (size_t b = 0; b < sizeof(batch_sizes)/sizeof(batch_sizes[0]); b++) {
        for (size_t d = 0; d < sizeof(dims)/sizeof(dims[0]); d++) {
            adv_batch_churn_config_t config = {
                .batch_size = batch_sizes[b],
                .vector_dim = dims[d],
                .n_iterations = iterations,
                .use_arena = (strcmp(alloc->name, "palloc") == 0)
            };
            
            adv_result_t result;
            result.samples = malloc(ADV_MAX_SAMPLES * sizeof(uint64_t));
            
            adv_batch_churn_benchmark(&result, alloc, &config);
            adv_print_result(&result, ADV_OUTPUT_TABLE);
            
            free(result.samples);
            free((void*)result.variant);
        }
    }
}

static void run_simd_latency_suite(const adv_allocator_t* alloc, size_t iterations) {
    const size_t dims[] = {ADV_DIM_384, ADV_DIM_768, ADV_DIM_1536, ADV_DIM_4096};
    const bool cold_cache_options[] = {false, true};
    const bool unaligned_options[] = {false, true};
    
    for (size_t d = 0; d < sizeof(dims)/sizeof(dims[0]); d++) {
        for (size_t c = 0; c < sizeof(cold_cache_options)/sizeof(cold_cache_options[0]); c++) {
            for (size_t u = 0; u < sizeof(unaligned_options)/sizeof(unaligned_options[0]); u++) {
                adv_simd_latency_config_t config = {
                    .vector_dim = dims[d],
                    .n_iterations = iterations,
                    .cold_cache = cold_cache_options[c],
                    .unaligned_access = unaligned_options[u]
                };
                
                adv_result_t result;
                result.samples = malloc(ADV_MAX_SAMPLES * sizeof(uint64_t));
                
                adv_simd_latency_benchmark(&result, alloc, &config);
                adv_print_result(&result, ADV_OUTPUT_TABLE);
                
                free(result.samples);
                free((void*)result.variant);
            }
        }
    }
}

static void run_tlb_pressure_suite(const adv_allocator_t* alloc, size_t iterations) {
    const size_t working_sets[] = {1, 2, 4, 8};  /* GB */
    const size_t dims[] = {ADV_DIM_768, ADV_DIM_1536};
    const size_t strides[] = {1, 4, 16};  /* MB */
    const bool thp_options[] = {false, true};
    
    for (size_t ws = 0; ws < sizeof(working_sets)/sizeof(working_sets[0]); ws++) {
        for (size_t d = 0; d < sizeof(dims)/sizeof(dims[0]); d++) {
            for (size_t s = 0; s < sizeof(strides)/sizeof(strides[0]); s++) {
                for (size_t t = 0; t < sizeof(thp_options)/sizeof(thp_options[0]); t++) {
                    adv_tlb_pressure_config_t config = {
                        .working_set_gb = working_sets[ws],
                        .vector_dim = dims[d],
                        .stride_mb = strides[s],
                        .use_thp = thp_options[t]
                    };
                    
                    adv_result_t result;
                    result.samples = malloc(ADV_MAX_SAMPLES * sizeof(uint64_t));
                    
                    adv_tlb_pressure_benchmark(&result, alloc, &config);
                    adv_print_result(&result, ADV_OUTPUT_TABLE);
                    
                    free(result.samples);
                    free((void*)result.variant);
                }
            }
        }
    }
}

static void run_allocator_suite(const adv_allocator_t* alloc, const char* scenario, 
                                 size_t iterations, adv_output_fmt_t output_fmt) {
    printf("\n");
    printf("================================================================\n");
    printf("Running adversarial benchmarks: %s\n", alloc->name);
    printf("================================================================\n");
    
    if (output_fmt == ADV_OUTPUT_TABLE) {
        adv_print_table_header();
    } else if (output_fmt == ADV_OUTPUT_CSV) {
        adv_print_csv_header();
    } else if (output_fmt == ADV_OUTPUT_MARKDOWN) {
        adv_print_markdown_header();
    }
    
    if (strcmp(scenario, "all") == 0 || strcmp(scenario, "batch_churn") == 0) {
        run_batch_churn_suite(alloc, iterations);
    }
    
    if (strcmp(scenario, "all") == 0 || strcmp(scenario, "simd_latency") == 0) {
        run_simd_latency_suite(alloc, iterations);
    }
    
    if (strcmp(scenario, "all") == 0 || strcmp(scenario, "tlb_pressure") == 0) {
        run_tlb_pressure_suite(alloc, iterations);
    }
}

int main(int argc, char** argv) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Default options */
    char* alloc_name = "all";
    char* scenario = "all";
    adv_output_fmt_t output_fmt = ADV_OUTPUT_TABLE;
    size_t iterations = 10000;
    int nthreads = 1;
    
    /* Parse command line */
    static struct option long_options[] = {
        {"alloc",       required_argument, 0, 'a'},
        {"scenario",    required_argument, 0, 's'},
        {"output",      required_argument, 0, 'o'},
        {"iterations",  required_argument, 0, 'i'},
        {"threads",     required_argument, 0, 't'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "a:s:o:i:t:h", long_options, NULL)) != -1) {
        switch (opt) {
        case 'a':
            alloc_name = optarg;
            break;
        case 's':
            scenario = optarg;
            break;
        case 'o':
            if (strcmp(optarg, "table") == 0) output_fmt = ADV_OUTPUT_TABLE;
            else if (strcmp(optarg, "csv") == 0) output_fmt = ADV_OUTPUT_CSV;
            else if (strcmp(optarg, "json") == 0) output_fmt = ADV_OUTPUT_JSON;
            else if (strcmp(optarg, "markdown") == 0) output_fmt = ADV_OUTPUT_MARKDOWN;
            else {
                fprintf(stderr, "Invalid output format: %s\n", optarg);
                return 1;
            }
            break;
        case 'i':
            iterations = atoi(optarg);
            break;
        case 't':
            nthreads = atoi(optarg);
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }
    
    printf("Adversarial Vector Benchmark Suite\n");
    printf("====================================\n");
    printf("Allocator: %s\n", alloc_name);
    printf("Scenario: %s\n", scenario);
    printf("Iterations: %zu\n", iterations);
    printf("Threads: %d\n", nthreads);
    printf("Output format: %d\n", output_fmt);
    printf("\n");
    
    /* Run benchmarks for selected allocator(s) */
    if (strcmp(alloc_name, "all") == 0) {
        run_allocator_suite(&adv_system_allocator, scenario, iterations, output_fmt);
        run_allocator_suite(&adv_palloc_allocator, scenario, iterations, output_fmt);
        run_allocator_suite(&adv_mimalloc_allocator, scenario, iterations, output_fmt);
        run_allocator_suite(&adv_jemalloc_allocator, scenario, iterations, output_fmt);
        run_allocator_suite(&adv_tcmalloc_allocator, scenario, iterations, output_fmt);
    } else if (strcmp(alloc_name, "system") == 0) {
        run_allocator_suite(&adv_system_allocator, scenario, iterations, output_fmt);
    } else if (strcmp(alloc_name, "palloc") == 0) {
        run_allocator_suite(&adv_palloc_allocator, scenario, iterations, output_fmt);
    } else if (strcmp(alloc_name, "mimalloc") == 0) {
        run_allocator_suite(&adv_mimalloc_allocator, scenario, iterations, output_fmt);
    } else if (strcmp(alloc_name, "jemalloc") == 0) {
        run_allocator_suite(&adv_jemalloc_allocator, scenario, iterations, output_fmt);
    } else if (strcmp(alloc_name, "tcmalloc") == 0) {
        run_allocator_suite(&adv_tcmalloc_allocator, scenario, iterations, output_fmt);
    } else {
        fprintf(stderr, "Unknown allocator: %s\n", alloc_name);
        return 1;
    }
    
    printf("\nBenchmark suite completed.\n");
    return 0;
}