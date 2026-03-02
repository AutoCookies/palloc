# palloc Performance Guide

This document summarizes palloc’s benchmark profile, the recommended build for maximum throughput, and an **improvement roadmap** for scenarios where palloc currently lags (to be addressed with targeted C-core changes when applicable).

---

## 1. Build for maximum throughput

Use a **Release** build with **no** padding, secure, or debug options so hot paths stay minimal:

```bash
# Option A: CMake preset (recommended)
cmake --preset release-maxperf
cmake --build build

# Option B: Explicit options
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DPA_OPT_ARCH=ON \
  -DPA_PADDING=OFF \
  -DPA_SECURE=OFF \
  -DPA_DEBUG=OFF
cmake --build build
```

Then run the benchmark from the **same** build (bench links against this library):

```bash
cd bench && cmake -B build -DPALLOC_ROOT=.. && cmake --build build
cd build && bash ../run_bench.sh
```

**Options that hurt throughput if enabled:** `PA_PADDING`, `PA_SECURE`, `PA_DEBUG` / `PA_DEBUG_INTERNAL` / `PA_DEBUG_FULL`. Keep them OFF for benchmarking.

---

## 2. Where palloc wins today

| Scenario              | Strength | Notes |
|-----------------------|----------|--------|
| **alloc_free_batch**  | ★ #1     | Batch alloc/free; palloc’s free-list and page design excel here. |
| **cross_thread**      | Top tier | Alloc on one thread, free on another; palloc’s cross-thread handling is strong. |
| **fragmentation_churn** | ★ #1  | Mixed alloc/free patterns; good fit for palloc’s sharding and reuse. |
| **object_pool**       | Strong   | Pool-like usage benefits from palloc’s small-block and page layout. |

These align with palloc’s design: **batch allocation**, **cross-thread** workloads, and **fragmentation-heavy** patterns.

---

## 3. Where palloc currently lags (and why)

| Scenario               | Gap vs leader | Likely cause (C-core) |
|------------------------|---------------|---------------------------|
| **calloc_bench**       | ~100× slower  | **Done:** faster zeroing (rep stosb, AVX2, __builtin_memset); freshly committed OS memory is marked so `free_is_zero` is set and we skip memset on first use (segment commit → page `is_zero_init`). |
| **alloc_free_mt**      | ~2× slower    | **Done:** delayed free is processed in batches (`PA_DELAYED_FREE_BATCH`) to reduce contention when many threads free to the same heap. |
| **alloc_free_same_thread** | ~1.5×  | Same-thread hot path; could benefit from even leaner small-block path and cache layout. |
| **realloc_bench**      | ~5–6× slower  | **Done:** in-place shrink (up to 25% waste); aligned copy; in-place expand when the block is large and the segment has contiguous free slices (`_pa_segment_try_extend_span`). |
| **latency_large**      | ~3–4× slower  | Large allocations go through generic path and possibly more locking/segment work; large-block path and contention are the levers. |
| **latency_small**      | ~1.5× slower  | Same as same_thread: small-block and cache layout. |
| **mixed_sizes**        | ~5× slower    | Many size classes; could stress size-class selection, fragmentation, or cache. Tuning small/medium bins and locality would help. |
| **peak_rss**           | Much slower   | Measures speed of driving RSS up; likely limited by segment commit or growth policy. |
| **thread_scale**       | ~3× slower    | Similar to alloc_free_mt; per-thread scalability and shared structure contention. |

Remaining gaps require further **C-core** changes (heap.c, segment.c, page.c, etc.); some calloc and realloc improvements are already in place.

---

## 4. Improvement roadmap (C-core) — implemented

The following C-core changes have been applied (RAII/Bigtech style: clear ownership, init/cleanup symmetry, production checks):

1. **calloc**  
   - **Done:** All freshly committed OS memory is marked via `pa_segment_commit(..., out_committed_is_zero)` → `page->is_zero_init = segment->free_is_zero || committed_is_zero`, so `free_is_zero` is set and we skip memset on first use.  
   - Small-size calloc already benefits from the same zero-skip when the block is from a known-zero page.

2. **realloc**  
   - **Done:** In-place expand when the block is large (multi-slice) and the segment has contiguous free space: `_pa_segment_try_extend_span` extends the page’s span and we zero only the new tail.  
   - Alloc+copy+free path uses aligned copy and immediate free; no further change.

3. **alloc_free_mt / thread_scale**  
   - **Done:** Per-call processing of cross-thread delayed frees is capped at `PA_DELAYED_FREE_BATCH` (32) in `_pa_heap_delayed_free_partial`; remainder is re-queued to reduce contention.

4. **mixed_sizes / latency_small / same_thread**  
   - Hot path remains in `_pa_page_malloc_zero` with prefetch and branch hints; size-class and bin layout unchanged.

5. **latency_large / peak_rss**  
   - Large allocations already commit the span in one shot in `pa_segment_span_allocate`; segment growth is unchanged.

---

## 5. Summary

- **Best throughput build:** Use the `release-maxperf` preset or the equivalent options above; keep padding, secure, and debug off.  
- **Current strengths:** alloc_free_batch, cross_thread, fragmentation_churn, object_pool.  
- **Current weaknesses:** mixed_sizes, latency_*, peak_rss, thread_scale may still benefit from further tuning; see Section 4 for what is already implemented.
