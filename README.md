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

### Building on Linux / macOS

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
sudo cmake --install .
```

Using Ninja for a faster build:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

### Building on Windows

**Using Visual Studio (MSVC):**

1. Open a **Developer Command Prompt for VS** or ensure `cl` and `cmake` are on `PATH`.
2. From the repo root:

```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
cmake --install build --prefix C:\palloc-install
```

For 32-bit use `-A Win32`; for ARM64 use `-A ARM64`.

**Using MinGW (GCC on Windows):**

```cmd
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build
cmake --install build
```

**Windows DLL override:** To use palloc as a drop-in replacement (override `malloc`/`free`) when building the **shared** library, the build uses a redirect DLL. If **bin/** does not already contain `palloc-redirect.dll` and `palloc-redirect.lib`, they are **built from source** (see [bin/BUILD.md](bin/BUILD.md)). To build without the redirect (no override), use:

```cmd
cmake -B build -DPA_WIN_REDIRECT=OFF ...
```

Static and shared libraries will still be built; only the runtime redirect for overriding other DLLs is disabled.

**Effective build options (Linux and Windows):**

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Release | `Release`, `Debug`, `RelWithDebInfo` |
| `CMAKE_INSTALL_PREFIX` | system-dependent | Install location (e.g. `/usr/local`, `C:\palloc`) |
| `PA_BUILD_SHARED` | ON | Build shared library (`.so` / `.dll`) |
| `PA_BUILD_STATIC` | ON | Build static library (`.a` / `.lib`) |
| `PA_BUILD_TESTS` | ON | Build test executables |
| `PA_OVERRIDE` | ON | Enable malloc/free override (see platform notes above) |
| `PA_WIN_REDIRECT` | ON (Windows) | Use redirect DLL for override on Windows; set OFF if `bin/` redirect DLLs are missing |
| `PA_OPT_ARCH` | OFF (x64), ON (arm64) | Architecture-specific optimizations (e.g. arm64: fast atomics) |
| `PA_PADDING` | OFF | Extra padding per block (detect overflow; costs throughput) |
| `PA_SECURE` | OFF | Security mitigations (costs throughput) |
| `PA_DEBUG` / `PA_DEBUG_FULL` | OFF | Assertions and invariant checks (costs throughput) |

**Maximum throughput (benchmarks):** Use the `release-maxperf` preset so padding, secure, and debug are off and `PA_OPT_ARCH` is on:

```bash
cmake --preset release-maxperf && cmake --build build
```

See [doc/PERFORMANCE.md](doc/PERFORMANCE.md) for the full performance guide and an improvement roadmap for weak scenarios.

Run `cmake -B build -L` to list all cache variables.

CI builds and tests palloc on **Linux** (Ninja) and **Windows** (MSVC and MinGW) on every push; see [.github/workflows/build.yml](.github/workflows/build.yml).

### Usage

- **Linux/macOS:** Link with `libpalloc` and include `<palloc.h>`. For process-wide override without recompiling, use `LD_PRELOAD` (Linux) or `DYLD_INSERT_LIBRARIES` (macOS).
- **Windows:** Link with `palloc.lib` (or use the static library), include `<palloc.h>`, and ensure `palloc.dll` (and, if using override, `palloc-redirect.dll`) are next to your executable. See [bin/readme.md](bin/readme.md) for override details.

```bash
# Example (Linux/macOS)
gcc -o myprogram myfile.c -lpalloc

# Example (Windows, MSVC: link with palloc and ensure palloc.dll is in the same folder)
cl myfile.c /I path\to\palloc-install\include path\to\palloc-install\lib\palloc.lib
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
| alloc_free_batch                   | ops/sec    |     9.86 Mop/s |    19.28 Mop/s |    15.20 Mop/s |     2.62 Mop/s |     8.95 Mop/s |
| alloc_free_batch                   | p99 ns     |       970.0 ns |       456.0 ns |       471.0 ns |        2.96 µs |        2.42 µs |
| alloc_free_batch                   | RSS        |       31.6 MiB |       19.3 MiB |       19.9 MiB |       17.7 MiB |       24.7 MiB |
| alloc_free_mt                      | ops/sec    |    15.26 Mop/s |     1.36 Mop/s |     6.99 Mop/s |     6.20 Mop/s |    15.44 Mop/s |
| alloc_free_mt                      | p99 ns     |       100.0 ns |       500.0 ns |       200.0 ns |       100.0 ns |       100.0 ns |
| alloc_free_mt                      | RSS        |       48.1 MiB |       39.3 MiB |       43.9 MiB |       43.9 MiB |       52.6 MiB |
| alloc_free_same_thread             | ops/sec    |    23.35 Mop/s |    10.05 Mop/s |     8.86 Mop/s |    17.96 Mop/s |     7.32 Mop/s |
| alloc_free_same_thread             | p99 ns     |       100.0 ns |       300.0 ns |       300.0 ns |       100.0 ns |       100.0 ns |
| alloc_free_same_thread             | RSS        |       21.6 MiB |       11.3 MiB |       11.5 MiB |        9.9 MiB |       16.6 MiB |
| calloc_bench                       | ops/sec    |    21.77 Mop/s |   302.20 Kop/s |   147.25 Kop/s |     4.21 Mop/s |    12.45 Mop/s |
| calloc_bench                       | p99 ns     |       100.0 ns |       12.10 µs |       29.80 µs |       200.0 ns |       100.0 ns |
| calloc_bench                       | RSS        |       32.0 MiB |       20.0 MiB |       20.1 MiB |       18.5 MiB |       25.5 MiB |
| cross_thread                       | ops/sec    |     2.41 Mop/s |     4.89 Mop/s |     8.99 Mop/s |   718.35 Kop/s |     3.90 Mop/s |
| cross_thread                       | p99 ns     |       500.0 ns |       500.0 ns |       400.0 ns |        2.90 µs |        1.10 µs |
| cross_thread                       | RSS        |       48.1 MiB |       39.3 MiB |       43.9 MiB |       43.9 MiB |       52.6 MiB |
| fragmentation_churn                | ops/sec    |     4.75 Mop/s |     7.55 Mop/s |    10.70 Mop/s |     2.09 Mop/s |     7.63 Mop/s |
| fragmentation_churn                | p99 ns     |       400.0 ns |       600.0 ns |       600.0 ns |        1.50 µs |       500.0 ns |
| fragmentation_churn                | RSS        |       32.1 MiB |       30.0 MiB |       30.1 MiB |       28.1 MiB |       36.0 MiB |
| latency_large                      | ops/sec    |     2.07 Mop/s |     2.19 Mop/s |     2.12 Mop/s |     1.51 Mop/s |     4.69 Mop/s |
| latency_large                      | p99 ns     |        1.10 µs |        1.30 µs |        1.50 µs |       500.0 ns |       700.0 ns |
| latency_large                      | RSS        |       32.0 MiB |       20.0 MiB |       20.1 MiB |       18.5 MiB |       25.5 MiB |
| latency_small                      | ops/sec    |    33.29 Mop/s |    17.41 Mop/s |    11.71 Mop/s |     7.78 Mop/s |     9.22 Mop/s |
| latency_small                      | p99 ns     |       100.0 ns |       300.0 ns |       300.0 ns |       100.0 ns |       100.0 ns |
| latency_small                      | RSS        |       31.6 MiB |       19.3 MiB |       19.9 MiB |       17.7 MiB |       24.7 MiB |
| mixed_sizes                        | ops/sec    |    16.14 Mop/s |     1.09 Mop/s |     3.01 Mop/s |    11.22 Mop/s |    15.83 Mop/s |
| mixed_sizes                        | p99 ns     |       200.0 ns |        1.10 µs |        1.70 µs |       100.0 ns |       100.0 ns |
| mixed_sizes                        | RSS        |       32.1 MiB |       39.3 MiB |       43.9 MiB |       36.2 MiB |       44.5 MiB |
| object_pool                        | ops/sec    |    22.27 Mop/s |     5.44 Mop/s |     9.80 Mop/s |    13.49 Mop/s |    22.02 Mop/s |
| object_pool                        | p99 ns     |       100.0 ns |       300.0 ns |       200.0 ns |       100.0 ns |       100.0 ns |
| object_pool                        | RSS        |      106.9 MiB |      115.4 MiB |      116.2 MiB |       65.7 MiB |      131.0 MiB |
| peak_rss                           | ops/sec    |     19.83 op/s |    126.93 op/s |      0.19 op/s |      2.69 op/s |     48.33 op/s |
| peak_rss                           | p99 ns     |       50.43 ms |        7.88 ms |         5.26 s |      371.66 ms |       20.69 ms |
| peak_rss                           | RSS        |        2.4 GiB |        2.2 GiB |        2.3 GiB |        2.0 GiB |        2.1 GiB |
| realloc_bench                      | ops/sec    |   524.45 Kop/s |   631.12 Kop/s |   306.16 Kop/s |     1.67 Mop/s |   608.55 Kop/s |
| realloc_bench                      | p99 ns     |        5.90 µs |       10.30 µs |       16.30 µs |       600.0 ns |        7.30 µs |
| realloc_bench                      | RSS        |       32.0 MiB |       23.9 MiB |       22.2 MiB |       18.5 MiB |       26.4 MiB |
| thread_scale                       | ops/sec    |    30.00 Mop/s |    10.00 Mop/s |    10.00 Mop/s |    60.00 Mop/s |    60.00 Mop/s |
| thread_scale                       | p99 ns     |       100.0 ns |       100.0 ns |       100.0 ns |       100.0 ns |       100.0 ns |
| thread_scale                       | RSS        |      106.9 MiB |      115.4 MiB |      116.2 MiB |       65.7 MiB |      131.0 MiB |



### Visual: Throughput by Scenario

#### Cross-Thread Performance (Throughput)
```
alloc_free_batch
        mimalloc  ██████████████████████████████  19.28 Mop/s ★
          palloc  ████████████████████████░░░░░░  15.20 Mop/s  
        jemalloc  ███████████████░░░░░░░░░░░░░░░  9.86 Mop/s  
        tcmalloc  ██████████████░░░░░░░░░░░░░░░░  8.95 Mop/s  
          system  ████░░░░░░░░░░░░░░░░░░░░░░░░░░  2.62 Mop/s  

  alloc_free_mt
        tcmalloc  ██████████████████████████████  15.44 Mop/s ★
        jemalloc  ██████████████████████████████  15.26 Mop/s  
          palloc  ██████████████░░░░░░░░░░░░░░░░  6.99 Mop/s  
          system  ████████████░░░░░░░░░░░░░░░░░░  6.20 Mop/s  
        mimalloc  ███░░░░░░░░░░░░░░░░░░░░░░░░░░░  1.36 Mop/s  

  alloc_free_same_thread
        jemalloc  ██████████████████████████████  23.35 Mop/s ★
          system  ███████████████████████░░░░░░░  17.96 Mop/s  
        mimalloc  █████████████░░░░░░░░░░░░░░░░░  10.05 Mop/s  
          palloc  ███████████░░░░░░░░░░░░░░░░░░░  8.86 Mop/s  
        tcmalloc  █████████░░░░░░░░░░░░░░░░░░░░░  7.32 Mop/s  

  calloc_bench
        jemalloc  ██████████████████████████████  21.77 Mop/s ★
        tcmalloc  █████████████████░░░░░░░░░░░░░  12.45 Mop/s  
          system  ██████░░░░░░░░░░░░░░░░░░░░░░░░  4.21 Mop/s  
        mimalloc  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  302.20 Kop/s  
          palloc  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  147.25 Kop/s  

  cross_thread
          palloc  ██████████████████████████████  8.99 Mop/s ★
        mimalloc  ████████████████░░░░░░░░░░░░░░  4.89 Mop/s  
        tcmalloc  █████████████░░░░░░░░░░░░░░░░░  3.90 Mop/s  
        jemalloc  ████████░░░░░░░░░░░░░░░░░░░░░░  2.41 Mop/s  
          system  ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░  718.35 Kop/s  

  fragmentation_churn
          palloc  ██████████████████████████████  10.70 Mop/s ★
        tcmalloc  █████████████████████░░░░░░░░░  7.63 Mop/s  
        mimalloc  █████████████████████░░░░░░░░░  7.55 Mop/s  
        jemalloc  █████████████░░░░░░░░░░░░░░░░░  4.75 Mop/s  
          system  ██████░░░░░░░░░░░░░░░░░░░░░░░░  2.09 Mop/s  

  latency_large
        tcmalloc  ██████████████████████████████  4.69 Mop/s ★
        mimalloc  ██████████████░░░░░░░░░░░░░░░░  2.19 Mop/s  
          palloc  ██████████████░░░░░░░░░░░░░░░░  2.12 Mop/s  
        jemalloc  █████████████░░░░░░░░░░░░░░░░░  2.07 Mop/s  
          system  ██████████░░░░░░░░░░░░░░░░░░░░  1.51 Mop/s  

  latency_small
        jemalloc  ██████████████████████████████  33.29 Mop/s ★
        mimalloc  ████████████████░░░░░░░░░░░░░░  17.41 Mop/s  
          palloc  ███████████░░░░░░░░░░░░░░░░░░░  11.71 Mop/s  
        tcmalloc  ████████░░░░░░░░░░░░░░░░░░░░░░  9.22 Mop/s  
          system  ███████░░░░░░░░░░░░░░░░░░░░░░░  7.78 Mop/s  

  mixed_sizes
        jemalloc  ██████████████████████████████  16.14 Mop/s ★
        tcmalloc  █████████████████████████████░  15.83 Mop/s  
          system  █████████████████████░░░░░░░░░  11.22 Mop/s  
          palloc  ██████░░░░░░░░░░░░░░░░░░░░░░░░  3.01 Mop/s  
        mimalloc  ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░  1.09 Mop/s  

  object_pool
        jemalloc  ██████████████████████████████  22.27 Mop/s ★
        tcmalloc  ██████████████████████████████  22.02 Mop/s  
          system  ██████████████████░░░░░░░░░░░░  13.49 Mop/s  
          palloc  █████████████░░░░░░░░░░░░░░░░░  9.80 Mop/s  
        mimalloc  ███████░░░░░░░░░░░░░░░░░░░░░░░  5.44 Mop/s  

  peak_rss
        mimalloc  ██████████████████████████████  126.93 op/s ★
        tcmalloc  ███████████░░░░░░░░░░░░░░░░░░░  48.33 op/s  
        jemalloc  █████░░░░░░░░░░░░░░░░░░░░░░░░░  19.83 op/s  
          system  █░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  2.69 op/s  
          palloc  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  0.19 op/s  

  realloc_bench
          system  ██████████████████████████████  1.67 Mop/s ★
        mimalloc  ███████████░░░░░░░░░░░░░░░░░░░  631.12 Kop/s  
        tcmalloc  ███████████░░░░░░░░░░░░░░░░░░░  608.55 Kop/s  
        jemalloc  █████████░░░░░░░░░░░░░░░░░░░░░  524.45 Kop/s  
          palloc  ██████░░░░░░░░░░░░░░░░░░░░░░░░  306.16 Kop/s  

  thread_scale
          system  ██████████████████████████████  60.00 Mop/s ★
        tcmalloc  ██████████████████████████████  60.00 Mop/s ★
        jemalloc  ███████████████░░░░░░░░░░░░░░░  30.00 Mop/s  
        mimalloc  █████░░░░░░░░░░░░░░░░░░░░░░░░░  10.00 Mop/s  
          palloc  █████░░░░░░░░░░░░░░░░░░░░░░░░░  10.00 Mop/s  


── Overall ranking (wins per allocator) ─────────────────────────────

  🥇 jemalloc       ████████░░░░░░░░░░░░  5/13 scenarios (38%)
  🥈 mimalloc       ███░░░░░░░░░░░░░░░░░  2/13 scenarios (15%)
  🥉 palloc         ███░░░░░░░░░░░░░░░░░  2/13 scenarios (15%)
     system         ███░░░░░░░░░░░░░░░░░  2/13 scenarios (15%)
     tcmalloc       ███░░░░░░░░░░░░░░░░░  2/13 scenarios (15%)
```

*Bars are scaled to the best result in each scenario. Results from a typical run on a multi-core Linux machine.*

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
