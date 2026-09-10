# PMU Hardware Counters & Performance Metrics Specification

## Overview

This document specifies the exact hardware performance monitoring units (PMU) to collect via `perf stat` to scientifically prove palloc's performance advantages over general-purpose allocators. These metrics are designed to expose architectural differences in memory allocation patterns, cache behavior, and TLB efficiency.

## Core PMU Counter Categories

### 1. CPU Pipeline Efficiency

#### Instructions Per Cycle (IPC)
**Purpose**: Measure overall CPU efficiency and identify pipeline stalls

**Metrics to collect**:
```bash
perf stat -e cycles,instructions,instructions/cycles \
    ./benchmark_binary
```

**Key insights**:
- Higher IPC indicates better pipeline utilization
- Low IPC suggests memory bottlenecks or branch mispredictions
- Compare palloc vs baselines to show reduced allocation overhead

**Expected palloc advantage**: 5-15% higher IPC in allocation-heavy workloads due to reduced branch misprediction and simpler allocation paths.

#### CPU Cycle Breakdown
**Purpose**: Identify where CPU cycles are spent

**Metrics to collect**:
```bash
perf stat -e cycles,cycles:k,cycles:u \
    ./benchmark_binary
```

- `cycles:k` - Kernel cycles (system time)
- `cycles:u` - User cycles (application time)

**Expected palloc advantage**: Lower kernel cycles due to reduced system call overhead and fewer page faults.

### 2. Cache Hierarchy Performance

#### L1 Data Cache
**Purpose**: Measure first-level cache efficiency for allocation metadata

**Metrics to collect**:
```bash
perf stat -e L1-dcache-loads,L1-dcache-load-misses,L1-dcache-load-misses/L1-dcache-loads \
    ./benchmark_binary
```

**Key metrics**:
- `L1-dcache-loads` - Total L1 data cache loads
- `L1-dcache-load-misses` - L1 cache miss count
- `L1-dcache-load-misses/L1-dcache-loads` - Miss rate

**Expected palloc advantage**: 10-20% lower L1 miss rate due to:
- Contiguous metadata layout
- Reduced pointer chasing
- Better spatial locality in free lists

#### Last Level Cache (LLC)
**Purpose**: Measure last-level cache pressure and memory bandwidth utilization

**Metrics to collect**:
```bash
perf stat -e LLC-loads,LLC-load-misses,LLC-load-misses/LLC-loads \
    ./benchmark_binary
```

**Additional memory bandwidth metrics**:
```bash
perf stat -e cache-references,cache-misses,cache-misses/cache-references \
    ./benchmark_binary
```

**Expected palloc advantage**: 15-30% lower LLC miss rate in batch allocation scenarios due to:
- Arena-based bulk allocation reducing metadata footprint
- Better page locality for vector data
- Reduced cross-CPU cache line bouncing

#### Cache-to-Cache Transfers
**Purpose**: Measure inter-core cache coherence traffic

**Metrics to collect**:
```bash
perf stat -e peer-cache,peer-cache/peer-cache+local-cache \
    ./benchmark_binary
```

**Expected palloc advantage**: Lower peer-cache rate due to sharded free lists reducing cross-thread cache line invalidations.

### 3. Translation Lookaside Buffer (TLB) Performance

#### Data TLB (dTLB)
**Purpose**: Measure TLB efficiency for memory accesses

**Metrics to collect**:
```bash
perf stat -e dTLB-loads,dTLB-load-misses,dTLB-load-misses/dTLB-loads \
    ./benchmark_binary
```

**Key insights**:
- High dTLB miss rate indicates TLB pressure
- General allocators with scattered allocations cause higher TLB pressure
- Arena allocators benefit from contiguous memory regions

**Expected palloc advantage**: 20-40% lower dTLB miss rate in large working-set benchmarks due to:
- Contiguous arena regions reducing page table entries
- Better huge-page utilization
- Reduced memory fragmentation

#### Instruction TLB (iTLB)
**Purpose**: Measure instruction fetch efficiency

**Metrics to collect**:
```bash
perf stat -e iTLB-loads,iTLB-load-misses,iTLB-load-misses/iTLB-loads \
    ./benchmark_binary
```

**Expected palloc advantage**: Minimal difference (code paths similar), but may show slight improvement due to simpler allocation logic.

### 4. Memory Access Patterns

#### Page Fault Analysis
**Purpose**: Measure virtual memory overhead

**Metrics to collect**:
```bash
perf stat -e page-faults,minor-faults,major-faults \
    ./benchmark_binary
```

**Key metrics**:
- `minor-faults` - Page faults resolved without disk I/O
- `major-faults` - Page faults requiring disk I/O

**Expected palloc advantage**: 30-50% fewer minor faults in batch allocation due to:
- Pre-allocated arena regions
- Reduced mmap/munmap calls
- Better page reuse

#### Memory Bandwidth
**Purpose**: Measure actual memory bandwidth utilization

**Metrics to collect**:
```bash
perf stat -e mem_inst_retired.all_loads,mem_inst_retired.all_stores \
    ./benchmark_binary
```

**For Intel processors with memory bandwidth counters**:
```bash
perf stat -e offcore_response.all_reads,offcore_response.all_writes \
    ./benchmark_binary
```

**Expected palloc advantage**: Lower memory bandwidth consumption due to reduced metadata overhead and better cache locality.

### 5. Branch Prediction

#### Branch Misprediction Rate
**Purpose**: Measure control flow efficiency

**Metrics to collect**:
```bash
perf stat -e branches,branch-misses,branch-misses/branches \
    ./benchmark_binary
```

**Expected palloc advantage**: 10-25% lower branch misprediction rate due to:
- Simpler allocation decision trees
- More predictable allocation patterns
- Reduced conditional branches in hot paths

### 6. Atomic Operations

#### Atomic Instruction Contention
**Purpose**: Measure lock contention in multi-threaded scenarios

**Metrics to collect**:
```bash
perf stat -e locks,lock contention \
    ./benchmark_binary
```

**For x86 with specific atomic counters**:
```bash
perf stat -e r1a1,r0d1 \
    ./benchmark_binary
```

**Expected palloc advantage**: 40-60% lower lock contention due to sharded free lists and per-thread arenas.

## Comprehensive Perf Command Templates

### Basic Memory Allocator Profiling
```bash
#!/bin/bash
# Basic allocator profiling script

ALLOCATOR=$1
BENCHMARK=$2
THREADS=${3:-8}

case $ALLOCATOR in
    palloc)
        export LD_PRELOAD=/usr/local/lib/libpalloc.so
        ;;
    system)
        export LD_PRELOAD=
        ;;
    jemalloc)
        export LD_PRELOAD=/usr/local/lib/libjemalloc.so
        ;;
    tcmalloc)
        export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libtcmalloc.so.4
        ;;
esac

perf stat -e cycles,instructions,instructions/cycles \
    -e L1-dcache-loads,L1-dcache-load-misses,L1-dcache-load-misses/L1-dcache-loads \
    -e LLC-loads,LLC-load-misses,LLC-load-misses/LLC-loads \
    -e dTLB-loads,dTLB-load-misses,dTLB-load-misses/dTLB-loads \
    -e page-faults,minor-faults \
    -e branches,branch-misses,branch-misses/branches \
    ./$BENCHMARK $THREADS
```

### Advanced PMU Collection with Output Parsing
```bash
#!/bin/bash
# Advanced PMU collection with JSON output

ALLOCATOR=$1
BENCHMARK=$2
THREADS=${3:-8}
OUTPUT_FILE="${BENCHMARK}_${ALLOCATOR}_perf.json"

# Set up allocator
case $ALLOCATOR in
    palloc)
        export LD_PRELOAD=/usr/local/lib/libpalloc.so
        ;;
    system)
        export LD_PRELOAD=
        ;;
esac

# Run perf with detailed output
perf stat -x, -o temp_perf.csv \
    -e cycles,instructions \
    -e L1-dcache-loads,L1-dcache-load-misses \
    -e LLC-loads,LLC-load-misses \
    -e dTLB-loads,dTLB-load-misses \
    -e page-faults,minor-faults \
    -e branches,branch-misses \
    ./$BENCHMARK $THREADS

# Parse and convert to JSON
python3 << EOF
import csv
import json

metrics = {}
with open('temp_perf.csv', 'r') as f:
    reader = csv.reader(f)
    for row in reader:
        if len(row) >= 2:
            event = row[2].strip()
            value = row[0].strip()
            try:
                metrics[event] = float(value)
            except ValueError:
                pass

result = {
    "allocator": "$ALLOCATOR",
    "benchmark": "$BENCHMARK",
    "threads": $THREADS,
    "metrics": metrics
}

with open('$OUTPUT_FILE', 'w') as f:
    json.dump(result, f, indent=2)

print(f"Results saved to $OUTPUT_FILE")
EOF

rm temp_perf.csv
```

### Continuous Monitoring During Benchmark
```bash
#!/bin/bash
# Continuous PMU monitoring during benchmark execution

ALLOCATOR=$1
BENCHMARK=$2
DURATION=${3:-30}

case $ALLOCATOR in
    palloc)
        export LD_PRELOAD=/usr/local/lib/libpalloc.so
        ;;
    system)
        export LD_PRELOAD=
        ;;
esac

# Start benchmark in background
./$BENCHMARK 8 &
BENCHMARK_PID=$!

# Monitor for specified duration
perf stat -e cycles,instructions,instructions/cycles \
    -e L1-dcache-loads,L1-dcache-load-misses \
    -e LLC-loads,LLC-load-misses \
    -e dTLB-loads,dTLB-load-misses \
    -e page-faults \
    -p $BENCHMARK_PID -- sleep $DURATION

# Kill benchmark if still running
kill $BENCHMARK_PID 2>/dev/null || true
wait $BENCHMARK_PID 2>/dev/null || true
```

## Platform-Specific PMU Counters

### Intel x86_64
```bash
# Intel-specific counters
perf stat -e r1a1,r0d1,r10e,r2a2 \
    ./benchmark_binary
```

- `r1a1` - MEM_LOAD_RETIRED.L1_HIT
- `r0d1` - MEM_LOAD_RETIRED.L2_HIT
- `r10e` - MEM_LOAD_RETIRED.L3_HIT
- `r2a2` - MEM_LOAD_RETIRED.L3_MISS

### AMD Zen Architecture
```bash
# AMD-specific counters
perf stat -e ls_ret_dc_ops,ls_ret_dc_misses \
    ./benchmark_binary
```

### ARM64 (AArch64)
```bash
# ARM64 PMU counters
perf stat -e l1d_cache,l1d_cache_refill,l1d_tlb_refill \
    ./benchmark_binary
```

## Memory Footprint Metrics

### RSS Tracking
**Purpose**: Measure actual memory consumption

**Method 1**: Using `/usr/bin/time`
```bash
/usr/bin/time -v ./benchmark_binary 8
```

**Method 2**: Using ps during execution
```bash
./benchmark_binary 8 &
PID=$!
while kill -0 $PID 2>/dev/null; do
    ps -o rss,vsz -p $PID
    sleep 0.1
done
```

**Method 3**: Using smem for more accurate memory accounting
```bash
sudo smem -c "name pid rss pss uss" --filter-name="benchmark"
```

### Fragmentation Analysis
**Purpose**: Calculate memory fragmentation ratio

**Formula**:
```
Fragmentation Ratio = (Peak RSS - Requested Bytes) / Requested Bytes
```

**Implementation**:
```c
#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>

void print_fragmentation(size_t requested_bytes) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    long peak_rss = ru.ru_maxrss * 1024; // Convert to bytes
    
    double frag_ratio = (double)(peak_rss - requested_bytes) / (double)requested_bytes;
    
    printf("Requested: %zu bytes\n", requested_bytes);
    printf("Peak RSS:  %ld bytes\n", peak_rss);
    printf("Fragmentation ratio: %.3f (%.1f%% overhead)\n", 
           frag_ratio, frag_ratio * 100.0);
}
```

## Adversarial Benchmark-Specific Metrics

### Vector Batch Churn
```bash
perf stat -e cycles,instructions \
    -e L1-dcache-loads,L1-dcache-load-misses \
    -e LLC-loads,LLC-load-misses \
    -e dTLB-loads,dTLB-load-misses \
    -e page-faults \
    ./adversarial_bench --alloc=palloc --scenario=batch_churn
```

**Key focus**: Cache miss rates and page fault reduction through arena allocation.

### SIMD Cacheline-Split & Access Latency
```bash
perf stat -e cycles,instructions,instructions/cycles \
    -e cache-references,cache-misses \
    -e branches,branch-misses \
    ./adversarial_bench --alloc=palloc --scenario=simd_latency
```

**Key focus**: IPC improvement and branch misprediction reduction with aligned memory.

### TLB Pressure
```bash
perf stat -e dTLB-loads,dTLB-load-misses,dTLB-load-misses/dTLB-loads \
    -e iTLB-loads,iTLB-load-misses \
    -e page-faults,minor-faults \
    ./adversarial_bench --alloc=palloc --scenario=tlb_pressure
```

**Key focus**: TLB miss rate reduction through contiguous memory allocation.

## Automated PMU Collection Suite

```bash
#!/bin/bash
# Automated PMU collection for all allocators and benchmarks

ALLOCATORS=("system" "palloc" "jemalloc" "tcmalloc")
BENCHMARKS=("sh6bench" "sh8bench" "alloc-test" "larson" "adversarial_bench")
THREADS=8
RESULTS_DIR="./pmu_results"

mkdir -p "$RESULTS_DIR"

for alloc in "${ALLOCATORS[@]}"; do
    for bench in "${BENCHMARKS[@]}"; do
        OUTPUT_FILE="${RESULTS_DIR}/${bench}_${alloc}_perf.txt"
        
        echo "Collecting PMU data: $alloc - $bench"
        
        case $alloc in
            palloc)
                export LD_PRELOAD=/usr/local/lib/libpalloc.so
                ;;
            system)
                export LD_PRELOAD=
                ;;
            jemalloc)
                export LD_PRELOAD=/usr/local/lib/libjemalloc.so
                ;;
            tcmalloc)
                export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libtcmalloc.so.4
                ;;
        esac
        
        perf stat -e cycles,instructions,instructions/cycles \
            -e L1-dcache-loads,L1-dcache-load-misses,L1-dcache-load-misses/L1-dcache-loads \
            -e LLC-loads,LLC-load-misses,LLC-load-misses/LLC-loads \
            -e dTLB-loads,dTLB-load-misses,dTLB-load-misses/dTLB-loads \
            -e page-faults,minor-faults \
            -e branches,branch-misses,branch-misses/branches \
            ./$bench $THREADS > "$OUTPUT_FILE" 2>&1
    done
done

echo "PMU collection completed. Results in: $RESULTS_DIR"
```

## Data Analysis and Visualization

### Calculating Relative Performance
```python
import json

def calculate_relative_speedup(baseline_metrics, test_metrics):
    """Calculate relative speedup given baseline and test metrics"""
    results = {}
    
    for metric in baseline_metrics:
        if metric in test_metrics:
            baseline = baseline_metrics[metric]
            test = test_metrics[metric]
            
            if baseline > 0:
                if metric in ['cycles', 'cache-misses', 'page-faults']:
                    # Lower is better
                    speedup = baseline / test
                else:
                    # Higher is better (instructions, cache-hits)
                    speedup = test / baseline
                
                results[metric] = speedup
    
    return results

# Example usage
with open('system_sh6bench_perf.json') as f:
    system_metrics = json.load(f)['metrics']

with open('palloc_sh6bench_perf.json') as f:
    palloc_metrics = json.load(f)['metrics']

speedup = calculate_relative_speedup(system_metrics, palloc_metrics)
print("Relative speedup:")
for metric, value in speedup.items():
    print(f"  {metric}: {value:.2f}x")
```

## Expected Performance Improvements Summary

| Metric Category | Expected Palloc Improvement | Reason |
|----------------|----------------------------|---------|
| IPC | 5-15% higher | Reduced allocation overhead, simpler code paths |
| L1 Cache Miss Rate | 10-20% lower | Better metadata locality, reduced pointer chasing |
| LLC Miss Rate | 15-30% lower | Arena-based allocation, better page locality |
| dTLB Miss Rate | 20-40% lower | Contiguous memory regions, huge-page utilization |
| Page Faults | 30-50% fewer | Pre-allocated arenas, reduced mmap calls |
| Branch Misprediction | 10-25% lower | Simpler allocation logic, predictable patterns |
| Lock Contention | 40-60% lower | Sharded free lists, per-thread arenas |
| Memory Footprint | 15-25% lower | Reduced fragmentation, better packing |

These PMU metrics provide a comprehensive, scientifically rigorous foundation for proving palloc's architectural advantages over general-purpose allocators.