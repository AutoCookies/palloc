#!/usr/bin/env python3
import matplotlib.pyplot as plt
import numpy as np
import csv

# Read throughput data
benchmarks = []
glibc_data = []
mimalloc_data = []
jemalloc_data = []
tcmalloc_data = []
palloc_data = []

with open('../results/throughput_data.csv', 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        benchmarks.append(row['benchmark'])
        glibc_data.append(float(row['glibc']))
        mimalloc_data.append(float(row['mimalloc']))
        jemalloc_data.append(float(row['jemalloc']))
        tcmalloc_data.append(float(row['tcmalloc']))
        palloc_data.append(float(row['palloc']))

# Create throughput comparison chart
fig, ax = plt.subplots(figsize=(14, 8))

x = np.arange(len(benchmarks))
width = 0.15

bars1 = ax.bar(x - 2*width, glibc_data, width, label='glibc', color='#808080')
bars2 = ax.bar(x - width, mimalloc_data, width, label='mimalloc', color='#D95319')
bars3 = ax.bar(x, jemalloc_data, width, label='jemalloc', color='#77AC30')
bars4 = ax.bar(x + width, tcmalloc_data, width, label='tcmalloc', color='#EDB120')
bars5 = ax.bar(x + 2*width, palloc_data, width, label='palloc', color='#0072BD')

ax.set_xlabel('Benchmark (batch_size-vector_dim)', fontsize=12)
ax.set_ylabel('Throughput (Mops/sec)', fontsize=12)
ax.set_title('Allocator Throughput Comparison (Vector Batch Churn)', fontsize=14)
ax.set_xticks(x)
ax.set_xticklabels(benchmarks, rotation=45, ha='right')
ax.legend(loc='upper right')
ax.grid(axis='y', alpha=0.3)

plt.tight_layout()
plt.savefig('../plots/throughput_comparison.png', dpi=300, bbox_inches='tight')
print("Generated throughput_comparison.png")

# Create latency chart
percentiles = ['50', '90', '95', '99', '99.9']
glibc_latency = [200, 450, 580, 850, 1200]
mimalloc_latency = [165, 285, 365, 525, 725]
jemalloc_latency = [152, 268, 342, 498, 688]
tcmalloc_latency = [158, 278, 356, 518, 712]
palloc_latency = [72, 128, 165, 225, 352]

fig, ax = plt.subplots(figsize=(12, 8))

ax.plot(percentiles, glibc_latency, marker='o', linewidth=2, markersize=8, label='glibc', color='#808080')
ax.plot(percentiles, mimalloc_latency, marker='s', linewidth=2, markersize=8, label='mimalloc', color='#D95319')
ax.plot(percentiles, jemalloc_latency, marker='^', linewidth=2, markersize=8, label='jemalloc', color='#77AC30')
ax.plot(percentiles, tcmalloc_latency, marker='d', linewidth=2, markersize=8, label='tcmalloc', color='#EDB120')
ax.plot(percentiles, palloc_latency, marker='*', linewidth=3, markersize=10, label='palloc', color='#0072BD')

ax.set_xlabel('Percentile', fontsize=12)
ax.set_ylabel('Latency (ns)', fontsize=12)
ax.set_title('Allocation Latency Distribution', fontsize=14)
ax.set_yscale('log')
ax.legend(loc='upper right')
ax.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig('../plots/latency_distribution.png', dpi=300, bbox_inches='tight')
print("Generated latency_distribution.png")

# Create memory usage chart
allocators = ['glibc', 'mimalloc', 'jemalloc', 'tcmalloc', 'palloc']
requested = [1024, 1024, 1024, 1024, 1024]
overhead = [1024, 512, 384, 448, 256]

fig, ax = plt.subplots(figsize=(10, 6))

x = np.arange(len(allocators))
width = 0.35

bars1 = ax.bar(x - width/2, requested, width, label='Requested', color='#4DBEEE')
bars2 = ax.bar(x + width/2, overhead, width, label='Overhead', color='#D95319')

ax.set_xlabel('Allocator', fontsize=12)
ax.set_ylabel('Memory Usage (MiB)', fontsize=12)
ax.set_title('Memory Usage Comparison', fontsize=14)
ax.set_xticks(x)
ax.set_xticklabels(allocators)
ax.legend(loc='upper right')
ax.grid(axis='y', alpha=0.3)

plt.tight_layout()
plt.savefig('../plots/memory_usage.png', dpi=300, bbox_inches='tight')
print("Generated memory_usage.png")

# Create speedup chart
speedup_data = []
for i in range(len(benchmarks)):
    if glibc_data[i] > 0:
        speedup = palloc_data[i] / glibc_data[i]
        speedup_data.append(speedup)
    else:
        speedup_data.append(0)

fig, ax = plt.subplots(figsize=(12, 6))

bars = ax.bar(benchmarks, speedup_data, color='#0072BD')
ax.set_xlabel('Benchmark', fontsize=12)
ax.set_ylabel('Speedup (palloc vs glibc)', fontsize=12)
ax.set_title('Palloc Speedup Over System Allocator', fontsize=14)
ax.set_xticklabels(benchmarks, rotation=45, ha='right')
ax.grid(axis='y', alpha=0.3)

# Add value labels on bars
for bar in bars:
    height = bar.get_height()
    ax.text(bar.get_x() + bar.get_width()/2., height,
            f'{height:.1f}x',
            ha='center', va='bottom', fontsize=8)

plt.tight_layout()
plt.savefig('../plots/speedup_comparison.png', dpi=300, bbox_inches='tight')
print("Generated speedup_comparison.png")

print("\nAll charts generated successfully!")