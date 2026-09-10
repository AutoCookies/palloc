# Palloc Benchmark Evaluation Blueprint
## Publication-Grade Memory Allocator Performance Assessment

**Version**: 1.0  
**Date**: 2026-09-10  
**Purpose**: Establish a world-class, publication-grade benchmark suite for objective evaluation of palloc against industry-standard allocators (glibc ptmalloc, mimalloc, jemalloc, tcmalloc) with emphasis on vector/embedding workloads and SIMD-optimized allocation patterns.

---

## Executive Summary

This blueprint provides a comprehensive framework for evaluating palloc's performance characteristics through two complementary benchmark approaches:

1. **Standard Industry Workloads**: Integration with the `mimalloc-bench` harness for objective comparison against established baselines
2. **Targeted Adversarial Benchmarks**: Custom stress tests designed to expose architectural limitations of general-purpose allocators when subjected to SIMD-aligned, batch-heavy vector workloads

The suite includes hardware-level performance monitoring (PMU counters), environment stabilization for reproducibility, and publication-ready visualization templates matching academic allocator paper standards.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Targeted Adversarial Vector Benchmarks](#targeted-adversarial-vector-benchmarks)
3. [Standard Industry Benchmark Coverage](#standard-industry-benchmark-coverage)
4. [PMU Hardware Counters & Metrics](#pmu-hardware-counters--metrics)
5. [Execution Rigor & Environment Stabilization](#execution-rigor--environment-stabilization)
6. [Visualization & Data Presentation](#visualization--data-presentation)
7. [Implementation Guide](#implementation-guide)
8. [Expected Performance Characteristics](#expected-performance-characteristics)
9. [Publication Guidelines](#publication-guidelines)

---

## Architecture Overview

### Benchmark Hierarchy

```
palloc-bench-suite/
├── adversarial/                    # Custom vector workloads
│   ├── adversarial_bench.h        # Benchmark interface and data structures
│   ├── adversarial_bench.c        # Core benchmark implementations
│   ├── adversarial_main.c         # Driver and test harness
│   └── CMakeLists.txt             # Build configuration
├── visualization/                  # Plot generation and reporting
│   ├── benchmark_report.tex       # LaTeX publication template
│   ├── plot_throughput.gnuplot    # Throughput comparison plots
│   ├── plot_latency.gnuplot       # Latency distribution plots
│   ├── plot_memory.gnuplot        # Memory usage/fragmentation plots
│   ├── plot_pmu.gnuplot           # PMU counter analysis plots
│   └── generate_plots.py          # Automated plot generation script
├── mimalloc_integration.md        # Standard benchmark integration guide
├── perf_metrics.md                # PMU counter specification
├── sanitize_environment.sh        # Environment stabilization script
└── BENCHMARK_BLUEPRINT.md         # This document
```

### Design Philosophy

The benchmark suite follows these core principles:

1. **Scientific Rigor**: All benchmarks include warmup phases, statistical sampling, and error bounds
2. **Reproducibility**: Environment stabilization scripts eliminate system-level variability
3. **Architectural Insight**: PMU counters expose microarchitectural behavior beyond wall-clock time
4. **Publication Standards**: Output formats match academic allocator research papers
5. **Real-World Relevance**: Workloads based on actual vector/embedding data pipeline patterns

---

## Targeted Adversarial Vector Benchmarks

### 1. Vector Batch Churn (Bulk Allocation & Free)

**Purpose**: Expose overhead of point-by-point deallocation vs. bulk arena reset in general-purpose allocators.

**Implementation**: `bench/adversarial/adversarial_bench.c` - `adv_batch_churn_benchmark()`

**Test Parameters**:
- Batch sizes: K = 32, 128, 512, 4096 vectors
- Vector dimensions: 384, 768, 1536, 4096 floats (~1.5 KB to 16 KB)
- Alignment: Strict 64-byte alignment (`aligned_alloc`/`posix_memalign`)
- Operations: Sequential batch allocation → batch deallocation (arena reset) vs. individual free

**Key Metrics**:
- Throughput (Mops/sec)
- Allocation latency per vector (p50, p90, p99)
- Memory fragmentation ratio: `(Peak RSS - Requested) / Requested`
- Cache miss rates (L1, LLC)

**Architectural Insight**:
- General allocators: O(N) freelist traversal with atomic operations per deallocation
- Palloc: O(1) arena reset with bulk reclamation
- Expected advantage: 2-4x throughput improvement in batch scenarios

**Execution Example**:
```bash
./adversarial_bench --alloc=palloc --scenario=batch_churn --iterations=50000
```

### 2. SIMD Cacheline-Split & Access Latency

**Purpose**: Measure performance impact of aligned vs. unaligned memory access patterns on SIMD operations.

**Implementation**: `bench/adversarial/adversarial_bench.c` - `adv_simd_latency_benchmark()`

**Test Parameters**:
- Vector dimensions: 384, 768, 1536, 4096 floats
- Cache states: Cold cache (flush with 256MB buffer) vs. hot cache
- Alignment patterns: 64-byte aligned vs. forced unaligned (1-byte offset)
- SIMD instruction sets: AVX2 (256-bit) vs. AVX-512 (512-bit) vs. scalar fallback

**Key Metrics**:
- Allocation + SIMD touch latency (nanoseconds)
- IPC (Instructions Per Cycle)
- Branch misprediction rate
- Cache miss penalties (cold vs. hot)

**Architectural Insight**:
- General allocators: Unpredictable alignment leading to cacheline splits
- Palloc: Guaranteed 64-byte alignment for AVX-512 operations
- Expected advantage: 30-50% lower latency in SIMD workloads

**Execution Example**:
```bash
./adversarial_bench --alloc=palloc --scenario=simd_latency --iterations=10000
```

### 3. Large Working-Set TLB Pressure

**Purpose**: Evaluate TLB efficiency under sustained memory pressure and huge-page utilization.

**Implementation**: `bench/adversarial/adversarial_bench.c` - `adv_tlb_pressure_benchmark()`

**Test Parameters**:
- Working set sizes: 1GB, 2GB, 4GB, 8GB
- Vector dimensions: 768, 1536 floats
- Stride patterns: 1MB, 4MB, 16MB between allocations
- Page configurations: 4KB pages vs. Transparent Huge Pages (THP)

**Key Metrics**:
- dTLB load miss rate
- iTLB load miss rate
- Page fault rate (minor faults)
- Memory bandwidth utilization
- Working set access latency

**Architectural Insight**:
- General allocators: Scattered allocations causing high TLB pressure
- Palloc: Contiguous arena regions reducing page table entries
- Expected advantage: 20-40% lower dTLB miss rate

**Execution Example**:
```bash
./adversarial_bench --alloc=palloc --scenario=tlb_pressure --iterations=1000
```

### Adversarial Benchmark Build & Execution

**Build Configuration**:
```bash
cd bench/adversarial
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Execution Options**:
```bash
# Test all allocators with all scenarios
./build/adversarial_bench --alloc=all --scenario=all --output=markdown

# Test specific configuration
./build/adversarial_bench --alloc=palloc --scenario=batch_churn \
    --iterations=50000 --output=csv
```

**Supported Allocators**:
- `palloc` (requires BENCH_USE_PALLOC)
- `system` (glibc ptmalloc)
- `mimalloc` (requires BENCH_USE_MIMALLOC)
- `jemalloc` (requires BENCH_USE_JEMALLOC)
- `tcmalloc` (requires BENCH_USE_TCMALLOC)

---

## Standard Industry Benchmark Coverage

### Mimalloc-Bench Integration

**Purpose**: Provide objective comparison against established industry workloads.

**Integration Guide**: See `bench/mimalloc_integration.md` for detailed instructions.

**Quick Start**:
```bash
# Build palloc with POSIX override
cmake -B build -DPALLOC_BUILD_MODE=USER -DPA_OVERRIDE=ON
cmake --build build
sudo cmake --install build

# Inject into mimalloc-bench via LD_PRELOAD
cd /path/to/mimalloc/benches
LD_PRELOAD=/usr/local/lib/libpalloc.so ./sh6bench 8
LD_PRELOAD=/usr/local/lib/libpalloc.so ./sh8bench 8
LD_PRELOAD=/usr/local/lib/libpalloc.so ./larson 8
```

### Standard Benchmark Categories

#### 1. Multi-thread Scaling & Lock Contention

**Benchmarks**: `sh6benchN`, `sh8benchN`, `alloc-testN`

**Purpose**: Measure throughput scaling under concurrent allocation pressure.

**Key Metrics**:
- Operations per second at thread counts 1, 2, 4, 8, 16, 32
- Lock contention metrics (via PMU counters)
- Cache coherence traffic

**Expected Palloc Advantage**: 15-25% higher throughput due to sharded free lists.

#### 2. Producer-Consumer Cross-Thread Free Contention

**Benchmarks**: `xmalloc-testN`, `rptestN`

**Purpose**: Stress test cross-thread allocation/deallocation patterns.

**Key Metrics**:
- Throughput under producer-consumer patterns
- Free list contention
- Cache line bouncing between threads

**Expected Palloc Advantage**: 20-30% higher throughput due to thread-local arenas.

#### 3. Long-Tail Fragmentation and Memory Bloating

**Benchmarks**: `larsonN`

**Purpose**: Measure memory fragmentation under sustained churn.

**Key Metrics**:
- Peak RSS vs. requested memory
- Fragmentation ratio
- Long-term memory growth

**Expected Palloc Advantage**: 25-35% lower memory overhead due to deterministic page layouts.

#### 4. Real-World Database/Server Proxy

**Benchmarks**: `redis` under `memtier_benchmark` traffic

**Purpose**: Evaluate performance in realistic server workloads.

**Key Metrics**:
- Requests per second
- Latency percentiles (p50, p99, p99.9)
- Memory footprint under sustained load

**Expected Palloc Advantage**: 10-20% higher throughput with 15-25% lower memory usage.

---

## PMU Hardware Counters & Metrics

### Comprehensive PMU Counter Specification

**Detailed Guide**: See `bench/perf_metrics.md` for complete PMU counter definitions and usage.

### Core Counter Categories

#### 1. CPU Pipeline Efficiency
```bash
perf stat -e cycles,instructions,instructions/cycles ./benchmark
```
- **IPC**: Overall CPU efficiency
- **Kernel vs User Cycles**: System call overhead

#### 2. Cache Hierarchy Performance
```bash
perf stat -e L1-dcache-loads,L1-dcache-load-misses \
    -e LLC-loads,LLC-load-misses ./benchmark
```
- **L1 Miss Rate**: Metadata locality
- **LLC Miss Rate**: Page locality and bandwidth

#### 3. TLB Performance
```bash
perf stat -e dTLB-loads,dTLB-load-misses \
    -e iTLB-loads,iTLB-load-misses ./benchmark
```
- **dTLB Miss Rate**: Working set pressure
- **iTLB Miss Rate**: Code locality

#### 4. Memory Access Patterns
```bash
perf stat -e page-faults,minor-faults,major-faults ./benchmark
```
- **Page Fault Rate**: Virtual memory overhead
- **Memory Bandwidth**: Actual DRAM utilization

#### 5. Branch Prediction
```bash
perf stat -e branches,branch-misses ./benchmark
```
- **Branch Misprediction Rate**: Control flow efficiency

#### 6. Atomic Operations
```bash
perf stat -e locks,lock_contention ./benchmark
```
- **Lock Contention**: Multi-threaded scaling

### Automated PMU Collection

**Comprehensive Collection Script**:
```bash
#!/bin/bash
# Collect PMU data for all allocators and benchmarks

ALLOCATORS=("system" "palloc" "jemalloc" "tcmalloc")
BENCHMARKS=("sh6bench" "sh8bench" "alloc-test" "larson" "adversarial_bench")

for alloc in "${ALLOCATORS[@]}"; do
    for bench in "${BENCHMARKS[@]}"; do
        # Set up allocator via LD_PRELOAD
        case $alloc in
            palloc) export LD_PRELOAD=/usr/local/lib/libpalloc.so ;;
            system) export LD_PRELOAD= ;;
            jemalloc) export LD_PRELOAD=/usr/local/lib/libjemalloc.so ;;
            tcmalloc) export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libtcmalloc.so.4 ;;
        esac
        
        # Collect comprehensive PMU data
        perf stat -e cycles,instructions,instructions/cycles \
            -e L1-dcache-loads,L1-dcache-load-misses \
            -e LLC-loads,LLC-load-misses \
            -e dTLB-loads,dTLB-load-misses \
            -e page-faults,minor-faults \
            -e branches,branch-misses \
            ./$bench 8 > "pmu_${bench}_${alloc}.txt" 2>&1
    done
done
```

---

## Execution Rigor & Environment Stabilization

### Production-Grade Environment Sanitization

**Script**: `bench/sanitize_environment.sh`

**Purpose**: Eliminate system-level variability sources for reproducible benchmarking.

### Sanitization Features

#### 1. CPU Performance Mode
```bash
sudo bash sanitize_environment.sh --performance-governor
```
- Lock CPU governor to performance mode
- Disable Intel Turbo Boost / AMD Turbo Core
- Disable CPU frequency scaling
- Disable CPU sleep states (C-states)

#### 2. Memory Configuration
```bash
sudo bash sanitize_environment.sh --disable-thp --set-swappiness=1
```
- Disable Transparent Huge Pages for fair comparison
- Set swappiness to minimum (1) to reduce swap activity
- Drop system caches before benchmark execution

#### 3. Address Space Layout
```bash
sudo bash sanitize_environment.sh --disable-aslr
```
- Disable Address Space Layout Randomization for consistent memory layouts

#### 4. System Services
```bash
sudo bash sanitize_environment.sh --stop-services
```
- Stop non-essential services (cron, syslog, bluetooth, etc.)
- Reduce background system noise

#### 5. I/O Configuration
- Set I/O scheduler to deadline/noop for consistent performance
- Disable NUMA balancing for consistent memory allocation

### Full Sanitization Workflow

```bash
# Apply all optimizations
sudo bash sanitize_environment.sh --full-sanitize

# Run benchmarks
./adversarial_bench --alloc=all --scenario=all

# Restore original settings
sudo bash sanitize_environment.sh --restore
```

### Benchmark Execution with Core Pinning

```bash
# Pin to specific CPU cores for consistent performance
sudo taskset -c 0-7 ./adversarial_bench --alloc=palloc --scenario=batch_churn

# Use NUMA-aware allocation if applicable
sudo numactl --cpunodebind=0 --membind=0 ./adversarial_bench --alloc=palloc
```

---

## Visualization & Data Presentation

### LaTeX Publication Template

**File**: `bench/visualization/benchmark_report.tex`

**Features**:
- Publication-grade formatting matching academic allocator papers
- Color scheme standardized across all visualizations
- Automated table generation with statistical formatting
- Integrated figure placement with proper captions

**Compilation**:
```bash
cd bench/visualization
pdflatex benchmark_report.tex
bibtex benchmark_report
pdflatex benchmark_report.tex
pdflatex benchmark_report.tex
```

### Gnuplot Plot Templates

#### 1. Throughput Comparison
```bash
gnuplot -c plot_throughput.gnuplot throughput_data.csv throughput.png "Allocator Throughput"
```

#### 2. Latency Distribution
```bash
gnuplot -c plot_latency.gnuplot latency_data.csv latency.png "Latency Distribution"
```

#### 3. Memory Usage
```bash
gnuplot -c plot_memory.gnuplot memory_data.csv memory.png "Memory Usage"
```

#### 4. PMU Analysis
```bash
gnuplot -c plot_pmu.gnuplot pmu_data.csv cache.png "Cache Performance" cache
gnuplot -c plot_pmu.gnuplot pmu_data.csv tlb.png "TLB Performance" tlb
```

### Automated Plot Generation

**Script**: `bench/visualization/generate_plots.py`

**Usage**:
```bash
python3 generate_plots.py --results-dir ./benchmark_results \
    --output-dir ./plots --format both --comparison
```

**Features**:
- Generates both Gnuplot and Matplotlib plots
- Creates comparison matrix tables
- Supports multiple output formats (PNG, PDF, SVG)
- Automated data parsing and validation

### Data Output Formats

#### CSV Format (Machine-readable)
```csv
allocator,scenario,variant,ops_per_sec,min_ns,p50_ns,p90_ns,p99_ns,max_ns,mean_ns,stddev_ns,rss_before,rss_after,rss_peak,requested_bytes,allocated_bytes,fragmentation_ratio
palloc,batch_churn,batch-128-dim-768,195.2,45,52,68,125,245,58.2,12.4,1048576,1310720,1310720,983040,1048576,0.067
```

#### JSON Format (Web-friendly)
```json
{
  "allocator": "palloc",
  "scenario": "batch_churn",
  "variant": "batch-128-dim-768",
  "ops_per_sec": 195.2,
  "min_ns": 45,
  "p50_ns": 52,
  "p90_ns": 68,
  "p99_ns": 125,
  "max_ns": 245,
  "mean_ns": 58.2,
  "stddev_ns": 12.4,
  "rss_before": 1048576,
  "rss_after": 1310720,
  "rss_peak": 1310720,
  "requested_bytes": 983040,
  "allocated_bytes": 1048576,
  "fragmentation_ratio": 0.067
}
```

#### Markdown Format (Human-readable)
```markdown
| Allocator     | Scenario          | Variant            | Throughput   | min ns | p50 ns | p90 ns | p99 ns | max ns | RSS      |
|---------------|-------------------|--------------------|--------------|--------|--------|--------|--------|--------|----------|
| palloc        | batch_churn       | batch-128-dim-768  | 195.2 Mop/s  | 45     | 52     | 68     | 125    | 245    | 1.3 MiB  |
```

---

## Implementation Guide

### Complete Benchmark Execution Workflow

#### Phase 1: Environment Setup
```bash
# 1. Install dependencies
sudo apt-get install -y cmake build-essential libjemalloc-dev libtcmalloc-dev \
    gnuplot python3-matplotlib texlive-latex-recommended texlive-latex-extra

# 2. Build palloc
cd /path/to/palloc
cmake -B build -DPALLOC_BUILD_MODE=USER -DPA_OVERRIDE=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build

# 3. Build adversarial benchmarks
cd bench/adversarial
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 4. Sanitize environment
cd /path/to/palloc/bench
sudo bash sanitize_environment.sh --full-sanitize
```

#### Phase 2: Run Standard Benchmarks
```bash
# 5. Clone and build mimalloc-bench
git clone https://github.com/microsoft/mimalloc.git
cd mimalloc/benches
bash build-bench-env.sh

# 6. Run mimalloc-bench with palloc
for bench in sh6bench sh8bench alloc-test larson; do
    LD_PRELOAD=/usr/local/lib/libpalloc.so ./$bench 8 > \
        /path/to/palloc/bench/results/${bench}_palloc.txt
done

# 7. Run baseline comparisons
for alloc in system jemalloc tcmalloc; do
    case $alloc in
        system) export LD_PRELOAD= ;;
        jemalloc) export LD_PRELOAD=/usr/local/lib/libjemalloc.so ;;
        tcmalloc) export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libtcmalloc.so.4 ;;
    esac
    
    for bench in sh6bench sh8bench alloc-test larson; do
        ./$bench 8 > /path/to/palloc/bench/results/${bench}_${alloc}.txt
    done
done
```

#### Phase 3: Run Adversarial Benchmarks
```bash
# 8. Run adversarial benchmarks
cd /path/to/palloc/bench/adversarial
for alloc in palloc system; do
    ./build/adversarial_bench --alloc=$alloc --scenario=all \
        --iterations=50000 --output=csv > \
        /path/to/palloc/bench/results/adversarial_${alloc}.csv
done
```

#### Phase 4: Collect PMU Data
```bash
# 9. Collect PMU counters for key benchmarks
cd /path/to/palloc/bench
for alloc in palloc system; do
    case $alloc in
        palloc) export LD_PRELOAD=/usr/local/lib/libpalloc.so ;;
        system) export LD_PRELOAD= ;;
    esac
    
    perf stat -e cycles,instructions,instructions/cycles \
        -e L1-dcache-loads,L1-dcache-load-misses \
        -e LLC-loads,LLC-load-misses \
        -e dTLB-loads,dTLB-load-misses \
        -e page-faults,minor-faults \
        -e branches,branch-misses \
        adversarial/build/adversarial_bench --alloc=$alloc \
        --scenario=batch_churn --iterations=10000 \
        > pmu_${alloc}.txt 2>&1
done
```

#### Phase 5: Generate Visualizations
```bash
# 10. Convert results to CSV format for plotting
# (Manual conversion or script to parse mimalloc-bench output)

# 11. Generate plots
cd /path/to/palloc/bench/visualization
python3 generate_plots.py --results-dir ../results --output-dir ../plots \
    --format both --comparison

# 12. Generate LaTeX report
pdflatex benchmark_report.tex
```

#### Phase 6: Cleanup
```bash
# 13. Restore environment
cd /path/to/palloc/bench
sudo bash sanitize_environment.sh --restore
```

### Continuous Integration Setup

**GitHub Actions Example**:
```yaml
name: Allocator Benchmark Suite

on: [push, pull_request]

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake build-essential libjemalloc-dev \
            gnuplot python3-matplotlib texlive-latex-recommended
      
      - name: Build palloc
        run: |
          cmake -B build -DPALLOC_BUILD_MODE=USER -DPA_OVERRIDE=ON
          cmake --build build -j$(nproc)
          sudo cmake --install build
      
      - name: Build adversarial benchmarks
        run: |
          cd bench/adversarial
          cmake -B build -S .
          cmake --build build
      
      - name: Run benchmarks
        run: |
          sudo bash bench/sanitize_environment.sh --full-sanitize
          cd bench/adversarial
          ./build/adversarial_bench --alloc=palloc --scenario=all \
            --iterations=10000 --output=csv
          sudo bash ../sanitize_environment.sh --restore
      
      - name: Generate plots
        run: |
          cd bench/visualization
          python3 generate_plots.py --results-dir ../adversarial \
            --output-dir ../plots --format gnuplot
```

---

## Expected Performance Characteristics

### Summary of Expected Improvements

| Metric Category | Expected Palloc Improvement | Primary Reason |
|----------------|----------------------------|----------------|
| **Throughput (Multi-threaded)** | 15-25% higher | Sharded free lists, reduced lock contention |
| **Memory Fragmentation** | 25-35% lower overhead | Deterministic page layouts, better size classes |
| **Batch Allocation** | 2-4x higher throughput | O(1) arena reset vs. O(N) individual frees |
| **SIMD Performance** | 30-50% lower latency | Guaranteed 64-byte alignment |
| **TLB Efficiency** | 20-40% lower miss rate | Contiguous arena regions |
| **Cache Performance** | 10-20% lower L1 miss rate | Better metadata locality |
| **LLC Performance** | 15-30% lower miss rate | Arena-based allocation patterns |
| **Page Fault Rate** | 30-50% fewer faults | Pre-allocated arenas, reduced mmap calls |
| **Branch Misprediction** | 10-25% lower rate | Simpler allocation logic |
| **Lock Contention** | 40-60% lower | Per-thread arenas, sharded free lists |

### Benchmark-Specific Expectations

#### Vector Batch Churn
- **Throughput**: 2-4x improvement over general allocators
- **Memory Overhead**: 50-70% lower fragmentation
- **Scaling**: Near-linear scaling to 32+ threads

#### SIMD Access Latency
- **Cold Cache**: 40-50% latency reduction
- **Hot Cache**: 30-40% latency reduction
- **Aligned Access**: 50-60% improvement vs. unaligned

#### TLB Pressure
- **dTLB Miss Rate**: 20-40% reduction at 8GB working set
- **Page Fault Rate**: 30-50% fewer minor faults
- **Memory Bandwidth**: 20-30% lower consumption

#### Standard Industry Workloads
- **sh6bench/sh8bench**: 15-25% higher throughput
- **Larson**: 25-35% lower memory overhead
- **Redis**: 10-20% higher throughput, 15-25% lower memory

---

## Publication Guidelines

### Academic Paper Presentation

When presenting benchmark results in academic papers, follow these guidelines:

#### 1. Result Presentation Format
- Use normalized performance (baseline = 1.0x) for easy comparison
- Include error bars/confidence intervals (95% confidence)
- Report both average and median values
- Include sample size (N) for statistical significance

#### 2. Table Format
```latex
\begin{table}[htbp]
\centering
\caption{Multi-threaded scaling performance (normalized to glibc = 1.0x)}
\begin{tabular}{l c c c c}
\toprule
\textbf{Allocator} & \textbf{sh6bench} & \textbf{sh8bench} & \textbf{alloc-test} & \textbf{Average} \\
\midrule
glibc (ptmalloc) & 1.00x & 1.00x & 1.00x & 1.00x \\
mimalloc & 1.45x & 1.52x & 1.38x & 1.45x \\
\rowcolor{pallocblue!20} \textbf{palloc} & \textbf{1.68x} & \textbf{1.75x} & \textbf{1.52x} & \textbf{1.65x} \\
\bottomrule
\end{tabular}
\end{table}
```

#### 3. Figure Format
- Use consistent color scheme across all figures
- Include proper axis labels with units
- Provide clear legends
- Use log scale for latency distributions
- Include sample size in captions

#### 4. Statistical Significance
- Run each benchmark at least 10 times
- Report mean ± standard deviation
- Use statistical tests (t-test, ANOVA) for significance claims
- Include confidence intervals in plots

#### 5. Environment Description
Always include complete environment specification:
- CPU model, core count, clock speed
- Memory size and type
- OS version and kernel
- Compiler version and flags
- Allocator versions and build options
- Environment stabilization settings

### Reproducibility Checklist

For publication, ensure your results are reproducible by:

1. **Environment Sanitization**: Document all sanitization steps
2. **Build Configuration**: Provide exact CMake flags and compiler versions
3. **Data Availability**: Make raw benchmark data available
4. **Script Publication**: Provide complete execution scripts
5. **Version Control**: Use git for benchmark suite and palloc versions
6. **Documentation**: Include this blueprint as supplementary material

### Ethical Considerations

- **Fair Comparison**: Use same build flags for all allocators
- **Representative Workloads**: Include both standard and adversarial benchmarks
- **Statistical Rigor**: Adequate sample sizes and confidence intervals
- **Transparency**: Report negative results as well as positive
- **Context**: Explain limitations and scope of results

---

## Conclusion

This benchmark evaluation blueprint provides a comprehensive, publication-grade framework for assessing palloc's performance characteristics. By combining standard industry workloads with targeted adversarial benchmarks, hardware-level PMU analysis, and rigorous environment stabilization, researchers can objectively demonstrate palloc's architectural advantages for vector/embedding workloads.

The suite is designed to be:

1. **Scientifically Rigorous**: Statistical sampling, error bounds, reproducible conditions
2. **Architecturally Insightful**: PMU counters expose microarchitectural behavior
3. **Publication-Ready**: Standardized formats matching academic standards
4. **Comprehensive**: Covers throughput, latency, memory efficiency, and scalability
5. **Extensible**: Easy to add new benchmarks and allocators

By following this blueprint, researchers can generate convincing, reproducible evidence of palloc's performance advantages in the specific domain of vector-intensive, SIMD-optimized workloads where general-purpose allocators are architecturally disadvantaged.

---

## Contact and Support

For questions, issues, or contributions to the benchmark suite:

- **Repository**: [palloc GitHub repository]
- **Issues**: GitHub issue tracker
- **Documentation**: This blueprint and inline code comments
- **Publications**: Cite this blueprint when using the benchmark suite

---

## Appendix A: Quick Reference Commands

### Build Commands
```bash
# Build palloc
cmake -B build -DPALLOC_BUILD_MODE=USER -DPA_OVERRIDE=ON
cmake --build build -j$(nproc)

# Build adversarial benchmarks
cd bench/adversarial
cmake -B build -S .
cmake --build build
```

### Environment Commands
```bash
# Full sanitization
sudo bash bench/sanitize_environment.sh --full-sanitize

# Restore environment
sudo bash bench/sanitize_environment.sh --restore

# Show current configuration
sudo bash bench/sanitize_environment.sh --show-config
```

### Benchmark Commands
```bash
# Adversarial benchmarks
./bench/adversarial/build/adversarial_bench --alloc=all --scenario=all

# Standard benchmarks with palloc
LD_PRELOAD=/usr/local/lib/libpalloc.so ./sh6bench 8

# PMU collection
perf stat -e cycles,instructions,L1-dcache-loads,L1-dcache-load-misses \
    ./benchmark_binary
```

### Visualization Commands
```bash
# Generate all plots
python3 bench/visualization/generate_plots.py \
    --results-dir ./results --output-dir ./plots --format both

# Generate LaTeX report
cd bench/visualization
pdflatex benchmark_report.tex
```

---

## Appendix B: Troubleshooting

### Common Issues

**Issue**: LD_PRELOAD not working
- **Solution**: Use absolute path to library, check for setuid binaries

**Issue**: Inconsistent benchmark results
- **Solution**: Run environment sanitization, disable frequency scaling

**Issue**: PMU counters not available
- **Solution**: Check perf permissions, ensure kernel supports counters

**Issue**: Build errors with allocators
- **Solution**: Verify allocator libraries are installed, check CMake configuration

**Issue**: Memory exhaustion in large benchmarks
- **Solution**: Reduce working set size, increase swap, run with fewer threads

---

**Document Version**: 1.0  
**Last Updated**: 2026-09-10  
**Maintained By**: Palloc Development Team