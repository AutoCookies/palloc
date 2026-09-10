#!/usr/bin/env python3
# generate_plots.py — Automated plot generation for allocator benchmark results
# ============================================================================
# This script generates all visualization plots from benchmark result data
# using both Gnuplot and Python matplotlib for comprehensive analysis.

import os
import sys
import subprocess
import json
import csv
import argparse
from pathlib import Path

# Get the directory where this script is located
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Try to import matplotlib, provide helpful error if not available
try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    import numpy as np
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False
    print("Warning: matplotlib not available. Only Gnuplot plots will be generated.")

# Color scheme matching LaTeX template
COLORS = {
    'palloc': '#0072BD',
    'glibc': '#808080', 
    'mimalloc': '#D95319',
    'jemalloc': '#77AC30',
    'tcmalloc': '#EDB120',
    'requested': '#4DBEEE',
    'overhead': '#D95319'
}

def check_gnuplot():
    """Check if gnuplot is available."""
    try:
        subprocess.run(['gnuplot', '--version'], capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False

def run_gnuplot(script_file, data_file, output_file, title="", metric_type="cache"):
    """Run a Gnuplot script with the given parameters."""
    if not check_gnuplot():
        print(f"Warning: gnuplot not available, skipping {output_file}")
        return False
    
    cmd = ['gnuplot', '-c', script_file, data_file, output_file, title, metric_type]
    try:
        subprocess.run(cmd, check=True, capture_output=True)
        print(f"Generated: {output_file}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"Error generating {output_file}: {e}")
        return False

def parse_csv_to_dict(csv_file):
    """Parse CSV file into dictionary format."""
    data = {}
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data[row[list(row.keys())[0]]] = row
    return data

def generate_matplotlib_throughput(data, output_file):
    """Generate throughput comparison plot using matplotlib."""
    if not MATPLOTLIB_AVAILABLE:
        return False
    
    allocators = ['glibc', 'mimalloc', 'jemalloc', 'tcmalloc', 'palloc']
    benchmarks = list(data.keys())
    
    fig, ax = plt.subplots(figsize=(12, 8))
    
    x = np.arange(len(benchmarks))
    width = 0.15
    multiplier = 0
    
    for allocator in allocators:
        values = [float(data[bench].get(allocator, 0)) for bench in benchmarks]
        offset = width * multiplier
        rects = ax.bar(x + offset, values, width, label=allocator, color=COLORS[allocator])
        multiplier += 1
    
    ax.set_xlabel('Benchmark', fontsize=12)
    ax.set_ylabel('Throughput (Mops/sec)', fontsize=12)
    ax.set_title('Allocator Throughput Comparison', fontsize=14)
    ax.set_xticks(x + width * 2)
    ax.set_xticklabels(benchmarks)
    ax.legend(loc='upper right')
    ax.grid(axis='y', alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Generated matplotlib plot: {output_file}")
    plt.close()
    return True

def generate_matplotlib_latency(data, output_file):
    """Generate latency distribution plot using matplotlib."""
    if not MATPLOTLIB_AVAILABLE:
        return False
    
    percentiles = ['50', '90', '95', '99', '99.9']
    allocators = ['glibc', 'mimalloc', 'jemalloc', 'tcmalloc', 'palloc']
    
    fig, ax = plt.subplots(figsize=(12, 8))
    
    for allocator in allocators:
        values = [float(data[p].get(allocator, 0)) for p in percentiles]
        ax.plot(percentiles, values, marker='o', linewidth=2, 
                markersize=8, label=allocator, color=COLORS[allocator])
    
    ax.set_xlabel('Percentile', fontsize=12)
    ax.set_ylabel('Latency (ns)', fontsize=12)
    ax.set_title('Allocation Latency Distribution', fontsize=14)
    ax.set_yscale('log')
    ax.legend(loc='upper right')
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Generated matplotlib plot: {output_file}")
    plt.close()
    return True

def generate_matplotlib_memory(data, output_file):
    """Generate memory usage comparison plot using matplotlib."""
    if not MATPLOTLIB_AVAILABLE:
        return False
    
    allocators = list(data.keys())
    requested = [float(data[alloc]['requested']) for alloc in allocators]
    overhead = [float(data[alloc]['overhead']) for alloc in allocators]
    
    fig, ax = plt.subplots(figsize=(12, 8))
    
    x = np.arange(len(allocators))
    width = 0.35
    
    rects1 = ax.bar(x - width/2, requested, width, label='Requested', color=COLORS['requested'])
    rects2 = ax.bar(x + width/2, overhead, width, label='Overhead', color=COLORS['overhead'])
    
    ax.set_xlabel('Allocator', fontsize=12)
    ax.set_ylabel('Memory Usage (MiB)', fontsize=12)
    ax.set_title('Memory Usage Comparison', fontsize=14)
    ax.set_xticks(x)
    ax.set_xticklabels(allocators)
    ax.legend(loc='upper right')
    ax.grid(axis='y', alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Generated matplotlib plot: {output_file}")
    plt.close()
    return True

def generate_comparison_matrix(results_dir, output_file):
    """Generate a markdown comparison matrix table."""
    allocators = ['glibc', 'mimalloc', 'jemalloc', 'tcmalloc', 'palloc']
    benchmarks = ['sh6bench', 'sh8bench', 'alloc-test', 'larson']
    
    with open(output_file, 'w') as f:
        f.write("# Allocator Performance Comparison Matrix\n\n")
        f.write("| Allocator | sh6bench | sh8bench | alloc-test | larson | Average |\n")
        f.write("|-----------|----------|----------|------------|--------|---------|\n")
        
        for alloc in allocators:
            results = []
            for bench in benchmarks:
                csv_file = os.path.join(results_dir, f"{bench}_results.csv")
                if os.path.exists(csv_file):
                    data = parse_csv_to_dict(csv_file)
                    value = data.get(bench, {}).get(alloc, 'N/A')
                    results.append(str(value))
                else:
                    results.append('N/A')
            
            # Calculate average if we have numeric values
            numeric_results = [float(r) for r in results if r != 'N/A']
            if numeric_results:
                avg = sum(numeric_results) / len(numeric_results)
                results.append(f"{avg:.2f}")
            else:
                results.append('N/A')
            
            f.write(f"| {alloc} | {' | '.join(results)} |\n")
    
    print(f"Generated comparison matrix: {output_file}")

def main():
    parser = argparse.ArgumentParser(description='Generate visualization plots for allocator benchmarks')
    parser.add_argument('--results-dir', required=True, help='Directory containing benchmark results')
    parser.add_argument('--output-dir', default='./plots', help='Output directory for plots')
    parser.add_argument('--format', choices=['gnuplot', 'matplotlib', 'both'], default='both',
                       help='Plot generation format')
    parser.add_argument('--comparison', action='store_true', help='Generate comparison matrix')
    
    args = parser.parse_args()
    
    # Create output directory
    os.makedirs(args.output_dir, exist_ok=True)
    
    # Get list of visualization scripts
    script_dir = SCRIPT_DIR
    
    results_dir = args.results_dir
    output_dir = args.output_dir
    
    print(f"Generating plots from results in: {results_dir}")
    print(f"Output directory: {output_dir}")
    
    # Generate throughput plots
    throughput_data = os.path.join(results_dir, 'throughput_results.csv')
    if os.path.exists(throughput_data):
        if args.format in ['gnuplot', 'both']:
            run_gnuplot(
                os.path.join(script_dir, 'plot_throughput.gnuplot'),
                throughput_data,
                os.path.join(output_dir, 'throughput_comparison.png'),
                "Allocator Throughput Comparison"
            )
        if args.format in ['matplotlib', 'both']:
            data = parse_csv_to_dict(throughput_data)
            generate_matplotlib_throughput(data, os.path.join(output_dir, 'throughput_comparison_matplotlib.png'))
    
    # Generate latency plots
    latency_data = os.path.join(results_dir, 'latency_results.csv')
    if os.path.exists(latency_data):
        if args.format in ['gnuplot', 'both']:
            run_gnuplot(
                os.path.join(script_dir, 'plot_latency.gnuplot'),
                latency_data,
                os.path.join(output_dir, 'latency_distribution.png'),
                "Allocation Latency Distribution"
            )
        if args.format in ['matplotlib', 'both']:
            data = parse_csv_to_dict(latency_data)
            generate_matplotlib_latency(data, os.path.join(output_dir, 'latency_distribution_matplotlib.png'))
    
    # Generate memory plots
    memory_data = os.path.join(results_dir, 'memory_results.csv')
    if os.path.exists(memory_data):
        if args.format in ['gnuplot', 'both']:
            run_gnuplot(
                os.path.join(script_dir, 'plot_memory.gnuplot'),
                memory_data,
                os.path.join(output_dir, 'memory_usage.png'),
                "Memory Usage Comparison"
            )
        if args.format in ['matplotlib', 'both']:
            data = parse_csv_to_dict(memory_data)
            generate_matplotlib_memory(data, os.path.join(output_dir, 'memory_usage_matplotlib.png'))
    
    # Generate PMU plots
    pmu_data = os.path.join(results_dir, 'pmu_results.csv')
    if os.path.exists(pmu_data):
        for metric_type in ['cache', 'tlb', 'branch', 'ipc']:
            if args.format in ['gnuplot', 'both']:
                run_gnuplot(
                    os.path.join(script_dir, 'plot_pmu.gnuplot'),
                    pmu_data,
                    os.path.join(output_dir, f'pmu_{metric_type}.png'),
                    f"PMU Analysis: {metric_type.upper()}",
                    metric_type
                )
    
    # Generate comparison matrix
    if args.comparison:
        generate_comparison_matrix(results_dir, os.path.join(output_dir, 'comparison_matrix.md'))
    
    print(f"\nPlot generation completed. Results in: {output_dir}")

if __name__ == '__main__':
    main()