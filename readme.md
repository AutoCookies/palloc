# palloc

<img align="left" width="100" height="100" src="./media/logo.png"/>

palloc is a general purpose allocator with excellent performance characteristics. Originally developed as `mimalloc`, it is designed to be a drop-in replacement for the standard `malloc` and can be used in projects without code changes.

## Key Features

- **Small and Consistent**: The library is about 10k LOC using simple and consistent data structures.
- **High Performance**: Outperforms many leading allocators in various benchmarks with low internal fragmentation.
- **Free List Sharding**: Uses multiple free lists per page to reduce contention and increase locality.
- **Secure**: Can be built in secure mode with guard pages and other mitigations.
- **First-Class Heaps**: Efficiently create and use multiple heaps across different threads.
- **Broad Compatibility**: Supports Windows, macOS, Linux, WASM, and various BSDs.

## Quick Start

### Building on Linux/macOS

```bash
mkdir -p build
cd build
cmake ..
make
sudo make install
```

### Usage

Link with `libpalloc` and include `<palloc.h>` (or use `LD_PRELOAD` for dynamic overriding).

```bash
# Example compilation
gcc -o myprogram myfile.c -lpalloc
```

For more detailed documentation, please refer to the `doc/` directory or visit the [project documentation](https://microsoft.github.io/palloc).

## Benchmark Results

The suite compares **palloc** against **system** (glibc), **mimalloc**, and **tcmalloc** across 13 scenarios (single-thread, multi-thread, mixed sizes, realloc, fragmentation, etc.). Run it yourself:

```bash
cd bench
bash run_bench.sh                    # full run (or --iter=50000 for a quick run)
# Outputs: build/results.csv, build/results_all.json, build/results_report.md
```

### Summary (wins per allocator)

| Allocator | Scenario wins | Share |
|-----------|---------------|-------|
| system    | 5/13          | 38%   |
| tcmalloc  | 5/13          | 38%   |
| mimalloc  | 2/13          | 15%   |
| **palloc**| **1/13**      | **8%** |

*palloc wins on **cross_thread** (alloc/free across threads), where it is designed to excel.*

### Throughput comparison (Mop/s — higher is better)

| Scenario                | mimalloc | palloc | system | tcmalloc |
|-------------------------|----------|--------|--------|----------|
| alloc_free_same_thread  | 24.0     | 20.5   | **58.5** | 52.2   |
| alloc_free_batch        | **60.9** | 34.9   | 18.2   | 39.9     |
| alloc_free_mt           | 19.7     | 25.6   | 37.9   | **40.3** |
| **cross_thread**       | 13.7     | **16.7** | 2.4  | 12.9     |
| latency_small           | 33.1     | 27.9   | **58.6** | 52.5   |
| latency_large           | 5.2      | 2.6    | 7.1    | **13.1** |
| fragmentation_churn    | 18.4     | 35.5   | 26.6   | **35.7** |
| mixed_sizes             | 13.8     | 6.7    | 44.9   | **55.4** |
| thread_scale            | 25.3     | 26.1   | 46.9   | **47.6** |

### Visual: throughput by scenario

**alloc_free_same_thread** (single-thread alloc/free)

```
  system   ████████████████████████████████████████████████████████████████  58.5 Mop/s
  tcmalloc ████████████████████████████████████████████████████████          52.2 Mop/s
  mimalloc ████████████████████                                              24.0 Mop/s
  palloc   ██████████████████                                                 20.5 Mop/s
```

**cross_thread** (alloc on one thread, free on another — palloc’s strength)

```
  palloc   ████████████████████████████████████████████████████████████████  16.7 Mop/s  ★
  mimalloc ████████████████████████████████████████████████                  13.7 Mop/s
  tcmalloc ██████████████████████████████████████████████                    12.9 Mop/s
  system   ████████                                                            2.4 Mop/s
```

**fragmentation_churn** (mixed alloc/free pattern)

```
  tcmalloc ████████████████████████████████████████████████████████████████  35.7 Mop/s
  palloc   ███████████████████████████████████████████████████████████████   35.5 Mop/s
  system   ██████████████████████████████████████                            26.6 Mop/s
  mimalloc ██████████████████████████                                       18.4 Mop/s
```

**mixed_sizes** (varying allocation sizes)

```
  tcmalloc ████████████████████████████████████████████████████████████████  55.4 Mop/s
  system   ██████████████████████████████████████████████████                44.9 Mop/s
  mimalloc ██████████████████                                                13.8 Mop/s
  palloc   ████████                                                           6.7 Mop/s
```

*Bars are scaled to the best result in each scenario. Results from a typical run (8 threads, 50k iterations); run the suite locally for your machine.*

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
