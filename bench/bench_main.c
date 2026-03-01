/* bench_main.c — Benchmark harness with CLI argument parsing
 * ============================================================
 * Usage:
 *   bench-<allocator> [options] [scenario...]
 *
 * Options:
 *   --threads=N        Number of worker threads (default: 4)
 *   --iter=N           Iterations per scenario (default: 1000000)
 *   --warmup=N         Warm-up iterations (default: 10000)
 *   --output=fmt       Output format: table | csv | json | markdown (default: table)
 *   --output-file=F    Write results to file F (default: stdout)
 *   --list             List available scenarios and exit
 *   --scenario=NAME    Run only the named scenario (repeatable)
 *   --no-color         Disable ANSI color output
 *
 * If no --scenario flags are given, all scenarios are run.
 */
#include "bench.h"
#include "bench_scenarios.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

/* -------------------------------------------------------------------------
 * CLI helpers
 * ---------------------------------------------------------------------- */
static int str_startswith(const char* s, const char* prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

static const char* str_after(const char* s, const char* prefix) {
  return s + strlen(prefix);
}

static void print_usage(const char* argv0) {
  fprintf(stderr,
    "Usage: %s [options] [scenario...]\n"
    "\n"
    "Options:\n"
    "  --threads=N          Worker threads          (default: 4)\n"
    "  --iter=N             Iterations per scenario (default: 1000000)\n"
    "  --warmup=N           Warm-up iterations      (default: 10000)\n"
    "  --output=FORMAT      table|csv|json|markdown (default: table)\n"
    "  --output-file=FILE   Write results to FILE   (default: stdout)\n"
    "  --list               List available scenarios\n"
    "  --scenario=NAME      Run only named scenario (repeatable)\n"
    "  --no-color           Disable ANSI colors\n"
    "\n"
    "Allocator: %s\n",
    argv0, bench_alloc_name());
}

#define MAX_FILTER 64

typedef struct {
  bench_config_t     config;
  bench_output_fmt_t fmt;
  char               output_file[256];
  char               filter[MAX_FILTER][64];
  int                filter_count;
  bool               list_only;
  bool               no_color;
} cli_args_t;

static void parse_args(int argc, char** argv, cli_args_t* a) {
  a->config.nthreads  = 4;
  a->config.iterations = 1000000;
  a->config.warmup    = 10000;
  a->config.size_lo   = 0; /* will be set per scenario */
  a->config.size_hi   = 0;
  a->fmt              = BENCH_OUTPUT_TABLE;
  a->output_file[0]   = '\0';
  a->filter_count     = 0;
  a->list_only        = false;
  a->no_color         = false;

  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      print_usage(argv[0]); exit(0);
    }
    else if (strcmp(arg, "--list") == 0)         { a->list_only = true; }
    else if (strcmp(arg, "--no-color") == 0)     { a->no_color = true; }
    else if (str_startswith(arg, "--threads="))  { a->config.nthreads   = atoi(str_after(arg, "--threads=")); }
    else if (str_startswith(arg, "--iter="))     { a->config.iterations = (size_t)atoll(str_after(arg, "--iter=")); }
    else if (str_startswith(arg, "--warmup="))   { a->config.warmup     = (size_t)atoll(str_after(arg, "--warmup=")); }
    else if (str_startswith(arg, "--output-file=")) {
      strncpy(a->output_file, str_after(arg, "--output-file="), sizeof(a->output_file) - 1);
    }
    else if (str_startswith(arg, "--output=")) {
      const char* fmt_str = str_after(arg, "--output=");
      if      (strcmp(fmt_str, "csv") == 0)      a->fmt = BENCH_OUTPUT_CSV;
      else if (strcmp(fmt_str, "json") == 0)     a->fmt = BENCH_OUTPUT_JSON;
      else if (strcmp(fmt_str, "markdown") == 0) a->fmt = BENCH_OUTPUT_MARKDOWN;
      else                                        a->fmt = BENCH_OUTPUT_TABLE;
    }
    else if (str_startswith(arg, "--scenario=")) {
      if (a->filter_count < MAX_FILTER) {
        strncpy(a->filter[a->filter_count++], str_after(arg, "--scenario="), 63);
      }
    }
    else {
      /* Positional: treat as scenario name filter */
      if (a->filter_count < MAX_FILTER) {
        strncpy(a->filter[a->filter_count++], arg, 63);
      }
    }
  }
}

static bool filter_matches(const cli_args_t* a, const char* name) {
  if (a->filter_count == 0) return true;
  for (int i = 0; i < a->filter_count; i++) {
    if (strcmp(a->filter[i], name) == 0) return true;
    /* substring match */
    if (strstr(name, a->filter[i]) != NULL) return true;
  }
  return false;
}

/* -------------------------------------------------------------------------
 * JSON envelope helpers
 * ---------------------------------------------------------------------- */
static void json_open(FILE* f, const cli_args_t* a) {
  time_t now; time(&now);
  char ts[64]; strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", gmtime(&now));
  fprintf(f, "{\n  \"meta\": {\n"
    "    \"allocator\": \"%s\",\n"
    "    \"threads\": %d,\n"
    "    \"iterations\": %zu,\n"
    "    \"warmup\": %zu,\n"
    "    \"timestamp\": \"%s\"\n"
    "  },\n  \"results\": [\n",
    bench_alloc_name(), a->config.nthreads,
    a->config.iterations, a->config.warmup, ts);
}
static void json_close(FILE* f) {
  /* Remove trailing comma if present by seeking back */
  fseek(f, -2, SEEK_CUR);
  fprintf(f, "\n  ]\n}\n");
}

/* -------------------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------------- */
int main(int argc, char** argv) {
  cli_args_t a;
  parse_args(argc, argv, &a);

  /* --list */
  if (a.list_only) {
    fprintf(stderr, "Available scenarios for allocator '%s':\n\n", bench_alloc_name());
    for (bench_scenario_entry_t* s = g_scenarios; s->name; s++) {
      fprintf(stderr, "  %-32s  %s\n", s->name, s->description);
    }
    return 0;
  }

  /* Redirect stdout to file if requested */
  FILE* out_file = stdout;
  if (a.output_file[0]) {
    out_file = fopen(a.output_file, "w");
    if (!out_file) { perror("Cannot open output file"); return 1; }
    /* dup2 so printf goes to file */
    dup2(fileno(out_file), fileno(stdout));
  }

  /* Print header (use stdout so JSON order is correct when redirected to file) */
  switch (a.fmt) {
  case BENCH_OUTPUT_CSV:      bench_print_csv_header();      break;
  case BENCH_OUTPUT_JSON:     json_open(stdout, &a);          break;
  case BENCH_OUTPUT_MARKDOWN: bench_print_markdown_header();  break;
  default:
    fprintf(stderr, BENCH_BOLD "\nPalloc Benchmark Suite — Allocator: %s\n" BENCH_RESET, bench_alloc_name());
    fprintf(stderr, "Threads: %d  Iterations: %zu  Warm-up: %zu\n\n",
      a.config.nthreads, a.config.iterations, a.config.warmup);
    bench_print_table_header();
    break;
  }

  /* Allocate sample buffer once and reuse */
  bench_result_t r;
  memset(&r, 0, sizeof(r));
  r.samples = (uint64_t*)malloc(BENCH_MAX_SAMPLES * sizeof(uint64_t));
  if (!r.samples) { fprintf(stderr, "OOM allocating sample buffer\n"); return 1; }
  r.alloc_name = bench_alloc_name();

  /* Run each scenario */
  for (bench_scenario_entry_t* s = g_scenarios; s->name; s++) {
    if (!filter_matches(&a, s->name)) continue;

    bench_config_t cfg = a.config;
    /* Scenario may override default sizes if caller didn't specify */
    if (cfg.size_lo == 0) cfg.size_lo = s->default_size_lo;
    if (cfg.size_hi == 0) cfg.size_hi = s->default_size_hi;
    if (cfg.size_lo == 0) cfg.size_lo = 8;
    if (cfg.size_hi == 0) cfg.size_hi = 256;

    if (a.fmt == BENCH_OUTPUT_TABLE) {
      fprintf(stderr, "  Running %-32s ...", s->name);
      fflush(stderr);
    }

    memset(r.samples, 0, sizeof(uint64_t) * BENCH_MAX_SAMPLES);
    r.n = 0;
    r.scenario = s->name;

    s->fn(&r, &cfg);
    bench_stats_compute(&r);

    if (a.fmt == BENCH_OUTPUT_TABLE) {
      fprintf(stderr, "\r");
      fflush(stderr);
    }

    bench_print_row(&r, a.fmt);
    fflush(stdout);
  }

  /* Footer */
  if (a.fmt == BENCH_OUTPUT_JSON) {
    fflush(stdout);
    json_close(stdout);
  }
  if (a.fmt == BENCH_OUTPUT_TABLE) {
    fprintf(stderr, "\nDone. Allocator: %s\n", bench_alloc_name());
  }

  free(r.samples);
  if (out_file != stdout) fclose(out_file);
  return 0;
}
