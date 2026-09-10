# Mimalloc-Bench Integration Guide for Palloc

## Overview

This guide provides step-by-step instructions for integrating palloc into the `mimalloc-bench` harness to run standard industry workloads and compare palloc against baseline allocators (mimalloc, tcmalloc, jemalloc, glibc, rpmalloc, snmalloc).

## Prerequisites

### 1. Clone and Build Mimalloc-Bench

```bash
# Clone mimalloc repository (includes benchmark suite)
git clone https://github.com/microsoft/mimalloc.git
cd mimalloc

# Build the benchmark environment
cd benches
bash build-bench-env.sh
```

This will:
- Download and build baseline allocators (jemalloc, tcmalloc, rpmalloc, snmalloc)
- Build standard benchmark binaries (sh6bench, sh8bench, alloc-test, etc.)
- Set up the benchmark environment structure

### 2. Build Palloc with POSIX Override

```bash
cd /path/to/palloc

# Build palloc with full POSIX malloc override
cmake -B build -DPALLOC_BUILD_MODE=USER \
    -DPA_OVERRIDE=ON \
    -DPA_BUILD_SHARED=ON \
    -DPA_BUILD_STATIC=OFF \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build -j$(nproc)

# Install to system (or set LD_LIBRARY_PATH)
sudo cmake --install build
```

## Integration Methods

### Method 1: LD_PRELOAD (Recommended for Quick Testing)

The simplest method is to use `LD_PRELOAD` to inject palloc into the benchmark binaries:

```bash
# Set path to palloc library
export LD_PRELOAD=/usr/local/lib/libpalloc.so

# Run any mimalloc-bench binary
cd /path/to/mimalloc/benches
./sh6bench 8
./sh8bench 8
./alloc-test 8
```

### Method 2: Build Custom Benchmarks Linked Against Palloc

For more rigorous testing, build custom benchmark binaries directly linked against palloc:

#### 2.1 Create Palloc-Specific Benchmarks

```bash
cd /path/to/mimalloc/benches

# Copy benchmark source files
mkdir -p palloc-bench
cp src/*.c palloc-bench/
cp include/*.h palloc-bench/
```

#### 2.2 Modify Benchmark Build Scripts

Create a custom build script for palloc:

```bash
#!/bin/bash
# build-palloc-bench.sh

BENCH_DIR="/path/to/mimalloc/benches"
PALLOC_DIR="/path/to/palloc"
BUILD_DIR="${BENCH_DIR}/build-palloc"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Compile benchmarks with palloc
gcc -O3 -march=native -I"${PALLOC_DIR}/include" \
    -L"${PALLOC_DIR}/build" \
    -lpalloc -lpthread -lm -lrt \
    "${BENCH_DIR}/src/sh6bench.c" -o sh6bench-palloc

gcc -O3 -march=native -I"${PALLOC_DIR}/include" \
    -L"${PALLOC_DIR}/build" \
    -lpalloc -lpthread -lm -lrt \
    "${BENCH_DIR}/src/sh8bench.c" -o sh8bench-palloc

gcc -O3 -march=native -I"${PALLOC_DIR}/include" \
    -L"${PALLOC_DIR}/build" \
    -lpalloc -lpthread -lm -lrt \
    "${BENCH_DIR}/src/alloc-test.c" -o alloc-test-palloc

echo "Palloc benchmarks built successfully"
```

### Method 3: Modify build-bench-env.sh

For permanent integration, modify the mimalloc `build-bench-env.sh` script:

```bash
# Add palloc to the list of allocators
ALLOCATORS="mimalloc jemalloc tcmalloc rpmalloc snmalloc palloc"

# Add palloc build function
build_palloc() {
    echo "Building palloc..."
    cd /path/to/palloc
    cmake -B build -DPALLOC_BUILD_MODE=USER -DPA_OVERRIDE=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc)
    sudo cmake --install build
}

# Modify the main build loop
for alloc in $ALLOCATORS; do
    case $alloc in
        palloc)
            build_palloc
            ;;
        *)
            # existing allocator builds
            ;;
    esac
done
```

## Standard Benchmark Coverage

### 1. Multi-thread Scaling & Lock Contention

**sh6benchN** (6-thread stress test):
```bash
# Run with palloc
LD_PRELOAD=/usr/local/lib/libpalloc.so ./sh6bench 8

# Run with system allocator for comparison
./sh6bench 8
```

**sh8benchN** (8-thread stress test):
```bash
LD_PRELOAD=/usr/local/lib/libpalloc.so ./sh8bench 8
```

**alloc-testN** (scalability test):
```bash
LD_PRELOAD=/usr/local/lib/libpalloc.so ./alloc-test 8
```

### 2. Producer-Consumer Cross-Thread Free Contention

**xmalloc-testN**:
```bash
LD_PRELOAD=/usr/local/lib/libpalloc.so ./xmalloc-test 8
```

**rptestN** (random pattern stress test):
```bash
LD_PRELOAD=/usr/local/lib/libpalloc.so ./rptest 8
```

### 3. Long-Tail Fragmentation and Memory Bloating

**larsonN** (Larson benchmark):
```bash
LD_PRELOAD=/usr/local/lib/libpalloc.so ./larson 8
```

### 4. Real-World Database/Server Proxy

**Redis benchmark**:
```bash
# Start redis with palloc
LD_PRELOAD=/usr/local/lib/libpalloc.so redis-server

# Run memtier_benchmark against it
memtier_benchmark -s localhost -p 6379 -t 8 -c 8 -n 100000
```

## Running the Full Benchmark Suite

Create a comprehensive benchmark runner script:

```bash
#!/bin/bash
# run_palloc_mimalloc_bench.sh

set -euo pipefail

MIMALLOC_BENCH_DIR="/path/to/mimalloc/benches"
PALLOC_LIB="/usr/local/lib/libpalloc.so"
RESULTS_DIR="./mimalloc-bench-results"
THREADS=8

mkdir -p "$RESULTS_DIR"

echo "Running mimalloc-bench suite with palloc"
echo "========================================"

run_benchmark() {
    local bench_name=$1
    local output_file="${RESULTS_DIR}/${bench_name}-palloc.txt"
    
    echo "Running: $bench_name"
    LD_PRELOAD="$PALLOC_LIB" \
        "${MIMALLOC_BENCH_DIR}/${bench_name}" "$THREADS" \
        > "$output_file" 2>&1 || true
    
    echo "Results saved to: $output_file"
}

# Core benchmarks
run_benchmark "sh6bench"
run_benchmark "sh8bench"
run_benchmark "alloc-test"
run_benchmark "xmalloc-test"
run_benchmark "rptest"
run_benchmark "larson"

echo "Benchmark suite completed"
echo "Results directory: $RESULTS_DIR"
```

## Performance Comparison Script

Create a comparison script to run palloc against all baselines:

```bash
#!/bin/bash
# compare_all_allocators.sh

set -euo pipefail

BENCH_DIR="/path/to/mimalloc/benches"
PALLOC_LIB="/usr/local/lib/libpalloc.so"
RESULTS_DIR="./allocator-comparison"
THREADS=8

mkdir -p "$RESULTS_DIR"

ALLOCATORS=(
    "system:LD_PRELOAD="
    "palloc:LD_PRELOAD=$PALLOC_LIB"
    "jemalloc:LD_PRELOAD=/usr/local/lib/libjemalloc.so"
    "tcmalloc:LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libtcmalloc.so.4"
)

BENCHMARKS=("sh6bench" "sh8bench" "alloc-test" "larson")

for alloc_info in "${ALLOCATORS[@]}"; do
    IFS=':' read -r alloc_name env_var <<< "$alloc_info"
    
    echo "Testing allocator: $alloc_name"
    
    for bench in "${BENCHMARKS[@]}"; do
        output_file="${RESULTS_DIR}/${bench}-${alloc_name}.txt"
        
        echo "  Running: $bench"
        env $env_var "${BENCH_DIR}/${bench}" "$THREADS" \
            > "$output_file" 2>&1 || true
    done
done

echo "Comparison completed. Results in: $RESULTS_DIR"
```

## Troubleshooting

### LD_PRELOAD Not Working

If `LD_PRELOAD` doesn't seem to work:

1. Check library path:
```bash
ldd ./sh6bench  # Verify palloc is being loaded
```

2. Use absolute path:
```bash
export LD_PRELOAD=$(realpath /usr/local/lib/libpalloc.so)
```

3. Check for setuid binaries (LD_PRELOAD is ignored):
```bash
ls -l ./sh6bench  # Look for setuid/setgid bits
```

### Build Errors

If you encounter build errors:

1. Ensure palloc is built with `PA_OVERRIDE=ON`
2. Check that palloc is built for the same architecture
3. Verify CMake version compatibility (3.18+)

### Performance Issues

If performance seems unexpected:

1. Verify CPU governor is set to performance:
```bash
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
```

2. Check for NUMA effects:
```bash
numactl --cpunodebind=0 --membind=0 ./sh6bench 8
```

3. Disable Transparent Huge Pages for fair comparison:
```bash
echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
```

## Advanced Configuration

### Custom Benchmark Parameters

Modify benchmark source files for custom parameters:

```c
// In sh6bench.c or similar
#define N_THREADS 16
#define OPERATIONS 10000000
#define MAX_SIZE 8192
```

### RSS and Memory Tracking

Use `/usr/bin/time` for detailed memory metrics:

```bash
/usr/bin/time -v LD_PRELOAD=/usr/local/lib/libpalloc.so ./sh6bench 8
```

### PMU Counter Collection

Combine with `perf` for hardware counter data:

```bash
perf stat -e cycles,instructions,cache-misses,cache-references,dTLB-load-misses \
    LD_PRELOAD=/usr/local/lib/libpalloc.so ./sh6bench 8
```

## Expected Results

When properly integrated, you should see:

1. **Multi-thread scaling**: Palloc should show superior scaling on sh6bench/sh8bench due to reduced lock contention
2. **Fragmentation**: Larson benchmark should show lower memory overhead for palloc
3. **Cross-thread contention**: xmalloc-test should benefit from palloc's free list sharding
4. **Real-world workloads**: Redis benchmarks should show improved throughput and lower latency

## Next Steps

After running the standard benchmarks, proceed to:

1. Run the adversarial vector benchmarks in `bench/adversarial/`
2. Collect PMU counter data using the provided perf scripts
3. Generate visualization reports using the LaTeX/Gnuplot templates
4. Analyze results to identify palloc's architectural advantages