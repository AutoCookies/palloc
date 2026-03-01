/* bench.h — Timer, statistics, and output helpers for palloc benchmark suite
 * =========================================================================
 * Include this header in all benchmark translation units.
 */
#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#ifdef __linux__
#  include <sys/resource.h>    /* getrusage */
#  include <sys/syscall.h>
#endif

/* -------------------------------------------------------------------------
 * Nanosecond wall-clock timer
 * ---------------------------------------------------------------------- */
static inline uint64_t bench_clock_ns(void) {
  struct timespec ts;
#ifdef CLOCK_MONOTONIC_RAW
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
  clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* -------------------------------------------------------------------------
 * RSS memory usage (bytes)
 * ---------------------------------------------------------------------- */
static inline long bench_rss_bytes(void) {
#ifdef __linux__
  struct rusage ru;
  getrusage(RUSAGE_SELF, &ru);
  return ru.ru_maxrss * 1024L;   /* Linux: kilobytes -> bytes */
#else
  return 0;
#endif
}

/* -------------------------------------------------------------------------
 * Statistics
 * ---------------------------------------------------------------------- */
#define BENCH_MAX_SAMPLES 1048576

typedef struct {
  const char* alloc_name;
  const char* scenario;
  uint64_t*   samples;      /* caller must malloc BENCH_MAX_SAMPLES * sizeof(uint64_t) */
  size_t      n;

  /* computed by bench_stats_compute() */
  double      ops_per_sec;
  uint64_t    min_ns;
  uint64_t    p50_ns;
  uint64_t    p90_ns;
  uint64_t    p99_ns;
  uint64_t    max_ns;
  double      mean_ns;
  double      stddev_ns;
  long        rss_bytes_before;
  long        rss_bytes_after;
} bench_result_t;

static int _bench_u64_cmp(const void* a, const void* b) {
  uint64_t x = *(const uint64_t*)a;
  uint64_t y = *(const uint64_t*)b;
  return (x > y) - (x < y);
}

static inline void bench_stats_compute(bench_result_t* r) {
  if (r->n == 0) return;
  qsort(r->samples, r->n, sizeof(uint64_t), _bench_u64_cmp);
  r->min_ns  = r->samples[0];
  r->max_ns  = r->samples[r->n - 1];
  r->p50_ns  = r->samples[(size_t)(r->n * 0.50)];
  r->p90_ns  = r->samples[(size_t)(r->n * 0.90)];
  r->p99_ns  = r->samples[(size_t)(r->n * 0.99)];

  double sum = 0;
  for (size_t i = 0; i < r->n; i++) sum += (double)r->samples[i];
  r->mean_ns = sum / (double)r->n;

  double var = 0;
  for (size_t i = 0; i < r->n; i++) {
    double d = (double)r->samples[i] - r->mean_ns;
    var += d * d;
  }
  r->stddev_ns = sqrt(var / (double)r->n);

  /* ops/sec = total_samples / total_seconds */
  double total_ns = sum;
  r->ops_per_sec = (total_ns > 0) ? ((double)r->n * 1e9 / total_ns) : 0.0;
}

/* -------------------------------------------------------------------------
 * Output
 * ---------------------------------------------------------------------- */
typedef enum {
  BENCH_OUTPUT_TABLE,
  BENCH_OUTPUT_CSV,
  BENCH_OUTPUT_JSON,
  BENCH_OUTPUT_MARKDOWN,
} bench_output_fmt_t;

/* Human-readable byte count */
static inline void bench_fmt_bytes(char* buf, size_t bufsz, long bytes) {
  if (bytes >= 1024L*1024*1024) snprintf(buf, bufsz, "%.1f GiB", (double)bytes / (1024.*1024*1024));
  else if (bytes >= 1024L*1024) snprintf(buf, bufsz, "%.1f MiB", (double)bytes / (1024.*1024));
  else if (bytes >= 1024L)      snprintf(buf, bufsz, "%.1f KiB", (double)bytes / 1024.);
  else                           snprintf(buf, bufsz, "%ld B",    bytes);
}

static inline void bench_fmt_ops(char* buf, size_t bufsz, double ops) {
  if (ops >= 1e9)      snprintf(buf, bufsz, "%.2f Gop/s", ops / 1e9);
  else if (ops >= 1e6) snprintf(buf, bufsz, "%.2f Mop/s", ops / 1e6);
  else if (ops >= 1e3) snprintf(buf, bufsz, "%.2f Kop/s", ops / 1e3);
  else                 snprintf(buf, bufsz, "%.2f op/s",  ops);
}

/* ANSI helpers */
#define BENCH_BOLD   "\033[1m"
#define BENCH_GREEN  "\033[32m"
#define BENCH_YELLOW "\033[33m"
#define BENCH_CYAN   "\033[36m"
#define BENCH_RESET  "\033[0m"

/* Print a single result row as a table line */
static inline void bench_print_row(const bench_result_t* r, bench_output_fmt_t fmt) {
  char ops_buf[32], rss_buf[32], rss_delta_buf[32];
  bench_fmt_ops(ops_buf, sizeof(ops_buf), r->ops_per_sec);
  bench_fmt_bytes(rss_buf, sizeof(rss_buf), r->rss_bytes_after);
  bench_fmt_bytes(rss_delta_buf, sizeof(rss_delta_buf),
                  r->rss_bytes_after - r->rss_bytes_before);

  switch (fmt) {
  case BENCH_OUTPUT_CSV:
    printf("%s,%s,%.2f,%llu,%llu,%llu,%llu,%llu,%.2f,%.2f,%ld,%ld\n",
      r->alloc_name, r->scenario,
      r->ops_per_sec,
      (unsigned long long)r->min_ns,
      (unsigned long long)r->p50_ns,
      (unsigned long long)r->p90_ns,
      (unsigned long long)r->p99_ns,
      (unsigned long long)r->max_ns,
      r->mean_ns, r->stddev_ns,
      r->rss_bytes_before, r->rss_bytes_after);
    break;
  case BENCH_OUTPUT_JSON:
    printf(
      "  {\"allocator\":\"%s\",\"scenario\":\"%s\","
      "\"ops_per_sec\":%.2f,"
      "\"min_ns\":%llu,\"p50_ns\":%llu,\"p90_ns\":%llu,\"p99_ns\":%llu,\"max_ns\":%llu,"
      "\"mean_ns\":%.2f,\"stddev_ns\":%.2f,"
      "\"rss_before\":%ld,\"rss_after\":%ld},\n",
      r->alloc_name, r->scenario, r->ops_per_sec,
      (unsigned long long)r->min_ns,
      (unsigned long long)r->p50_ns,
      (unsigned long long)r->p90_ns,
      (unsigned long long)r->p99_ns,
      (unsigned long long)r->max_ns,
      r->mean_ns, r->stddev_ns,
      r->rss_bytes_before, r->rss_bytes_after);
    break;
  case BENCH_OUTPUT_MARKDOWN:
    printf("| %-16s | %-32s | %12s | %8llu | %8llu | %8llu | %8llu | %8llu | %10s |\n",
      r->alloc_name, r->scenario, ops_buf,
      (unsigned long long)r->min_ns,
      (unsigned long long)r->p50_ns,
      (unsigned long long)r->p90_ns,
      (unsigned long long)r->p99_ns,
      (unsigned long long)r->max_ns,
      rss_buf);
    break;
  default: /* TABLE */
    printf(BENCH_CYAN "%-16s" BENCH_RESET " %-30s  "
           BENCH_GREEN "%12s" BENCH_RESET
           "  %6llu  %6llu  %6llu  %6llu  %8llu  %10s (+%s)\n",
      r->alloc_name, r->scenario, ops_buf,
      (unsigned long long)r->min_ns,
      (unsigned long long)r->p50_ns,
      (unsigned long long)r->p90_ns,
      (unsigned long long)r->p99_ns,
      (unsigned long long)r->max_ns,
      rss_buf, rss_delta_buf);
    break;
  }
}

static inline void bench_print_csv_header(void) {
  printf("allocator,scenario,ops_per_sec,min_ns,p50_ns,p90_ns,p99_ns,max_ns,mean_ns,stddev_ns,rss_before,rss_after\n");
}

static inline void bench_print_table_header(void) {
  printf(BENCH_BOLD
    "%-16s %-30s  %12s  %6s  %6s  %6s  %6s  %8s  %10s\n"
    BENCH_RESET,
    "Allocator", "Scenario", "Throughput",
    "min ns", "p50 ns", "p90 ns", "p99 ns", "max ns", "RSS (delta)");
  printf("%-16s %-30s  %12s  %6s  %6s  %6s  %6s  %8s  %10s\n",
    "----------------", "------------------------------",
    "------------", "------", "------", "------", "------", "--------", "----------");
}

static inline void bench_print_markdown_header(void) {
  printf("| %-16s | %-32s | %12s | %8s | %8s | %8s | %8s | %8s | %10s |\n",
    "Allocator", "Scenario", "Throughput", "min ns", "p50 ns", "p90 ns", "p99 ns", "max ns", "RSS");
  printf("|-%s-|-%s-|-%s:|-%s:|-%s:|-%s:|-%s:|-%s:|-%s:|\n",
    "----------------", "--------------------------------",
    "------------", "--------", "--------", "--------", "--------", "--------", "----------");
}
