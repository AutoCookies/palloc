# palloc

<img src="./media/logo.png"/>

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

### Benchmark Results

# palloc Benchmark Results

| Scenario                           | Metric     |       jemalloc |       mimalloc |         palloc |         system |       tcmalloc |
|------------------------------------|------------|----------------|----------------|----------------|----------------|----------------|
| alloc_free_batch                   | ops/sec    |    28.66 Mop/s |    25.39 Mop/s |    39.25 Mop/s |    14.29 Mop/s |    22.01 Mop/s |
| alloc_free_batch                   | p99 ns     |        64.0 ns |       262.0 ns |        39.0 ns |       114.0 ns |       132.0 ns |
| alloc_free_batch                   | RSS        |       31.8 MiB |       19.5 MiB |       19.9 MiB |       17.6 MiB |       25.3 MiB |
| alloc_free_mt                      | ops/sec    |    22.88 Mop/s |    15.74 Mop/s |    20.25 Mop/s |    39.34 Mop/s |     6.25 Mop/s |
| alloc_free_mt                      | p99 ns     |       100.0 ns |       200.0 ns |       100.0 ns |       100.0 ns |       200.0 ns |
| alloc_free_mt                      | RSS        |       48.3 MiB |       39.5 MiB |       43.9 MiB |       43.9 MiB |       52.0 MiB |
| alloc_free_same_thread             | ops/sec    |    43.11 Mop/s |    15.28 Mop/s |    18.95 Mop/s |    25.63 Mop/s |    30.03 Mop/s |
| alloc_free_same_thread             | p99 ns     |       100.0 ns |       100.0 ns |       100.0 ns |       100.0 ns |       100.0 ns |
| alloc_free_same_thread             | RSS        |       21.8 MiB |       11.5 MiB |       11.4 MiB |        9.9 MiB |       17.3 MiB |
| calloc_bench                       | ops/sec    |    28.59 Mop/s |   467.46 Kop/s |   348.90 Kop/s |    46.20 Mop/s |    30.71 Mop/s |
| calloc_bench                       | p99 ns     |       100.0 ns |        4.00 µs |        8.10 µs |       100.0 ns |       100.0 ns |
| calloc_bench                       | RSS        |       32.1 MiB |       20.1 MiB |       20.2 MiB |       18.5 MiB |       25.3 MiB |
| cross_thread                       | ops/sec    |     3.73 Mop/s |     7.22 Mop/s |    11.03 Mop/s |     1.78 Mop/s |     2.98 Mop/s |
| cross_thread                       | p99 ns     |       400.0 ns |       200.0 ns |       200.0 ns |        5.40 µs |        1.10 µs |
| cross_thread                       | RSS        |       48.3 MiB |       39.5 MiB |       43.9 MiB |       43.9 MiB |       52.0 MiB |
| fragmentation_churn                | ops/sec    |    16.43 Mop/s |    11.96 Mop/s |    17.27 Mop/s |    19.56 Mop/s |     5.93 Mop/s |
| fragmentation_churn                | p99 ns     |       200.0 ns |       300.0 ns |       200.0 ns |       500.0 ns |       300.0 ns |
| fragmentation_churn                | RSS        |       32.2 MiB |       30.1 MiB |       30.2 MiB |       28.1 MiB |       36.2 MiB |
| latency_large                      | ops/sec    |     2.67 Mop/s |     2.14 Mop/s |     3.31 Mop/s |    17.74 Mop/s |    11.48 Mop/s |
| latency_large                      | p99 ns     |       700.0 ns |       600.0 ns |        1.50 µs |       200.0 ns |       200.0 ns |
| latency_large                      | RSS        |       32.1 MiB |       20.1 MiB |       20.2 MiB |       18.5 MiB |       25.3 MiB |
| latency_small                      | ops/sec    |    41.73 Mop/s |    17.01 Mop/s |    33.90 Mop/s |    41.35 Mop/s |    23.25 Mop/s |
| latency_small                      | p99 ns     |       100.0 ns |       100.0 ns |       100.0 ns |       100.0 ns |       100.0 ns |
| latency_small                      | RSS        |       31.8 MiB |       19.5 MiB |       19.9 MiB |       17.6 MiB |       25.3 MiB |
| mixed_sizes                        | ops/sec    |    25.41 Mop/s |    12.18 Mop/s |     4.93 Mop/s |    42.82 Mop/s |     9.01 Mop/s |
| mixed_sizes                        | p99 ns     |       100.0 ns |       400.0 ns |        1.00 µs |       100.0 ns |       100.0 ns |
| mixed_sizes                        | RSS        |       32.3 MiB |       39.5 MiB |       43.8 MiB |       36.1 MiB |       44.3 MiB |
| object_pool                        | ops/sec    |    20.76 Mop/s |     5.25 Mop/s |    20.34 Mop/s |    29.45 Mop/s |     4.96 Mop/s |
| object_pool                        | p99 ns     |       100.0 ns |       200.0 ns |       100.0 ns |       100.0 ns |       100.0 ns |
| object_pool                        | RSS        |      125.0 MiB |      117.2 MiB |      108.8 MiB |       83.7 MiB |      149.6 MiB |
| peak_rss                           | ops/sec    |     26.58 op/s |    129.15 op/s |      0.66 op/s |      9.90 op/s |     69.04 op/s |
| peak_rss                           | p99 ns     |       37.63 ms |        7.74 ms |         1.51 s |      101.04 ms |       14.48 ms |
| peak_rss                           | RSS        |        2.4 GiB |        2.2 GiB |        2.3 GiB |        2.0 GiB |        2.2 GiB |
| realloc_bench                      | ops/sec    |   550.22 Kop/s |     1.21 Mop/s |   628.41 Kop/s |    14.44 Mop/s |   769.43 Kop/s |
| realloc_bench                      | p99 ns     |        6.10 µs |        3.80 µs |        5.70 µs |       200.0 ns |        4.40 µs |
| realloc_bench                      | RSS        |       32.1 MiB |       24.0 MiB |       22.2 MiB |       18.5 MiB |       26.6 MiB |
| thread_scale                       | ops/sec    |      0.00 op/s |    10.00 Mop/s |    20.00 Mop/s |      0.00 op/s |    60.00 Mop/s |
| thread_scale                       | p99 ns     |         0.0 ns |       100.0 ns |       100.0 ns |         0.0 ns |       100.0 ns |
| thread_scale                       | RSS        |      125.0 MiB |      117.2 MiB |      108.8 MiB |       83.7 MiB |      149.6 MiB |


### Visual: Throughput by Scenario

#### Cross-Thread Performance (Throughput)
*Alloc on one thread, free on another — where palloc is designed to excel.*

```mermaid
xychart-beta
    title "Cross-Thread Throughput (Mop/s)"
    x-axis ["palloc", "mimalloc", "jemalloc", "tcmalloc", "system"]
    y-axis "Mop/s" 0 12
    bar [11.03, 7.22, 3.73, 2.98, 1.78]
```

```
  palloc   ████████████████████████████████████████████████████████████████  11.03 Mop/s  ★
  mimalloc ██████████████████████████████████████████                        7.22 Mop/s
  jemalloc ██████████████████████                                            3.73 Mop/s
  tcmalloc █████████████████                                                 2.98 Mop/s
  system   ██████████                                                        1.78 Mop/s
```

#### Batch Allocation (Throughput)
*High-volume allocation and deallocation in batches.*

```mermaid
xychart-beta
    title "Batch Allocation Throughput (Mop/s)"
    x-axis ["palloc", "jemalloc", "mimalloc", "tcmalloc", "system"]
    y-axis "Mop/s" 0 45
    bar [39.25, 28.66, 25.39, 22.01, 14.29]
```

```
  palloc   ████████████████████████████████████████████████████████████████  39.25 Mop/s  ★
  jemalloc ██████████████████████████████████████████████                    28.66 Mop/s
  mimalloc █████████████████████████████████████████                         25.39 Mop/s
  tcmalloc ████████████████████████████████████                              22.01 Mop/s
  system   ███████████████████████                                           14.29 Mop/s
```

#### Other Scenarios (Peak Mop/s)

| Scenario                | Best Allocator | Throughput  |
|-------------------------|----------------|-------------|
| alloc_free_same_thread  | jemalloc       | 43.11 Mop/s |
| alloc_free_mt           | system         | 39.34 Mop/s |
| latency_small           | jemalloc       | 41.73 Mop/s |
| mixed_sizes             | system         | 42.82 Mop/s |
| thread_scale            | tcmalloc       | 60.00 Mop/s |

*Bars are scaled to the best result in each scenario. Results from a typical run on a multi-core Linux machine.*

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
