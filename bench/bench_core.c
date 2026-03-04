/*
 * Core API benchmark for dual-mode (USER and KERNEL).
 * Uses only pa_core_* and palloc_vector_*. USER: timing + printf. KERNEL: workload only.
 */
#include "palloc_backend_api.h"
#include "palloc/core_types.h"
#include "palloc/core_api.h"
#include <stddef.h>
#include <stdint.h>

#if defined(PALLOC_USER_MODE) || (defined(PALLOC_KERNEL_MODE) && defined(PALLOC_KERNEL_RUN_ON_HOST))
#include <stdio.h>
#include <time.h>
#ifdef __linux__
#define BENCH_CLOCK_ID  CLOCK_MONOTONIC
#else
#define BENCH_CLOCK_ID  CLOCK_MONOTONIC
#endif
static uint64_t bench_clock_ns(void)
{
  struct timespec ts;
  clock_gettime(BENCH_CLOCK_ID, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
#endif

#define BENCH_ITER_MALLOC   (500000u)
#define BENCH_ITER_VECTOR   (10000u)
#define BENCH_VECTOR_BYTES  (4096u)

static void run_malloc_free_loop(void)
{
  for (unsigned i = 0; i < BENCH_ITER_MALLOC; i++) {
    void *p = pa_core_malloc(64);
    if (p) {
      *(volatile char *)p = (char)i;
      pa_core_free(p);
    }
  }
}

static void run_vector_loop(void)
{
  for (unsigned i = 0; i < BENCH_ITER_VECTOR; i++) {
    palloc_vector_t vec;
    palloc_vector_init(&vec);
    if (palloc_vector_reserve(&vec, BENCH_VECTOR_BYTES) == 0) {
      palloc_vector_set_length(&vec, BENCH_VECTOR_BYTES / 2);
      palloc_vector_destroy(&vec);
    }
  }
}

#if defined(PALLOC_USER_MODE)
int main(void)
{
  palloc_backend_init();

  printf("bench-core (USER): core API only\n");
  printf("---------------------------------\n");

  uint64_t t0 = bench_clock_ns();
  run_malloc_free_loop();
  uint64_t t1 = bench_clock_ns();
  double sec = (double)(t1 - t0) * 1e-9;
  double ops = (sec > 0.0) ? ((double)BENCH_ITER_MALLOC / sec) : 0.0;
  printf("  malloc_free_loop: %u iters, %.3f s, %.2f Mop/s\n",
         BENCH_ITER_MALLOC, sec, ops / 1e6);

  t0 = bench_clock_ns();
  run_vector_loop();
  t1 = bench_clock_ns();
  sec = (double)(t1 - t0) * 1e-9;
  ops = (sec > 0.0) ? ((double)BENCH_ITER_VECTOR / sec) : 0.0;
  printf("  vector_loop:      %u iters, %.3f s, %.2f Kop/s\n",
         BENCH_ITER_VECTOR, sec, ops / 1e3);

  printf("---------------------------------\n");
  return 0;
}

#elif defined(PALLOC_KERNEL_MODE) && defined(PALLOC_KERNEL_RUN_ON_HOST)
/* KERNEL backend but running on host (mmap); print timing. */
int main(void)
{
  palloc_backend_init();
  printf("bench-core (KERNEL host-safe): core API only\n");
  printf("---------------------------------------------\n");
  uint64_t t0 = bench_clock_ns();
  run_malloc_free_loop();
  uint64_t t1 = bench_clock_ns();
  double sec = (double)(t1 - t0) * 1e-9;
  double ops = (sec > 0.0) ? ((double)BENCH_ITER_MALLOC / sec) : 0.0;
  printf("  malloc_free_loop: %u iters, %.3f s, %.2f Mop/s\n", BENCH_ITER_MALLOC, sec, ops / 1e6);
  t0 = bench_clock_ns();
  run_vector_loop();
  t1 = bench_clock_ns();
  sec = (double)(t1 - t0) * 1e-9;
  ops = (sec > 0.0) ? ((double)BENCH_ITER_VECTOR / sec) : 0.0;
  printf("  vector_loop:      %u iters, %.3f s, %.2f Kop/s\n", BENCH_ITER_VECTOR, sec, ops / 1e3);
  printf("---------------------------------------------\n");
  return 0;
}
#elif defined(PALLOC_KERNEL_MODE)
/* No stdio/timing; just run workload (for run under PoOS/QEMU or build check). */
int main(void)
{
  palloc_backend_init();
  run_malloc_free_loop();
  run_vector_loop();
  return 0;
}
#endif
