#!/usr/bin/env python3
"""
bench_report.py — Parse results_all.json and print a comparison table.

Usage:
  python3 bench_report.py [results_all.json]

The script renders:
  1. A per-scenario comparison table (all allocators side-by-side)
  2. ASCII bar charts for throughput and p99 latency
  3. A summary ranking for each category
"""

import json
import sys
import os
import math
from collections import defaultdict

# ── ANSI helpers ────────────────────────────────────────────────────────────
USE_COLOR = os.isatty(sys.stdout.fileno()) if hasattr(sys.stdout, 'fileno') else False

def c(code, s):
    return f"\033[{code}m{s}\033[0m" if USE_COLOR else s

bold   = lambda s: c("1", s)
green  = lambda s: c("32", s)
yellow = lambda s: c("33", s)
cyan   = lambda s: c("36", s)
red    = lambda s: c("31", s)

# ── Formatters ──────────────────────────────────────────────────────────────
def fmt_ops(v):
    if v >= 1e9:  return f"{v/1e9:.2f} Gop/s"
    if v >= 1e6:  return f"{v/1e6:.2f} Mop/s"
    if v >= 1e3:  return f"{v/1e3:.2f} Kop/s"
    return f"{v:.2f} op/s"

def fmt_ns(v):
    if v >= 1e9:  return f"{v/1e9:.2f} s"
    if v >= 1e6:  return f"{v/1e6:.2f} ms"
    if v >= 1e3:  return f"{v/1e3:.2f} µs"
    return f"{v:.1f} ns"

def fmt_bytes(v):
    if v >= 1 << 30: return f"{v/(1<<30):.1f} GiB"
    if v >= 1 << 20: return f"{v/(1<<20):.1f} MiB"
    if v >= 1 << 10: return f"{v/(1<<10):.1f} KiB"
    return f"{v} B"

def bar(value, max_value, width=30, char="█"):
    filled = int(round(value / max_value * width)) if max_value > 0 else 0
    return char * filled + "░" * (width - filled)

# ── Load data ────────────────────────────────────────────────────────────────
def load(path):
    with open(path) as f:
        data = json.load(f)
    # data is a list of allocator objects (from our merge step)
    # Each has { meta: {...}, results: [{allocator, scenario, ...}] }
    # Flatten into list of result rows
    rows = []
    for entry in data:
        if "results" in entry:
            for r in entry["results"]:
                rows.append(r)
        # Also handle plain list of result objects
        elif isinstance(entry, dict) and "allocator" in entry:
            rows.append(entry)
    return rows

# ── Group by scenario ────────────────────────────────────────────────────────
def group_by_scenario(rows):
    by_scenario = defaultdict(dict)
    for r in rows:
        alloc    = r.get("allocator", "?")
        scenario = r.get("scenario", "?")
        by_scenario[scenario][alloc] = r
    return by_scenario

# ── Print comparison table ────────────────────────────────────────────────────
def print_table(by_scenario, allocators):
    # Header
    alloc_hdr = "  ".join(f"{a:>14}" for a in allocators)
    print(bold(f"\n{'Scenario':<34}  {'Metric':<10}  {alloc_hdr}"))
    print("─" * (40 + 18 * len(allocators)))

    for scenario, alloc_map in sorted(by_scenario.items()):
        # ops/sec row
        ops_vals = {a: alloc_map[a].get("ops_per_sec", 0) for a in allocators if a in alloc_map}
        best_ops = max(ops_vals.values()) if ops_vals else 0

        ops_cells = []
        for a in allocators:
            if a not in alloc_map:
                ops_cells.append(f"{'N/A':>14}")
                continue
            v = alloc_map[a].get("ops_per_sec", 0)
            s = fmt_ops(v)
            if v == best_ops and best_ops > 0:
                ops_cells.append(green(f"{s:>14}") + "*")
            else:
                ops_cells.append(f"{s:>14} ")
        print(f"  {cyan(scenario):<34}  {'ops/sec':<10}  {'  '.join(ops_cells)}")

        # p99 latency row
        p99_vals = {a: alloc_map[a].get("p99_ns", float('inf')) for a in allocators if a in alloc_map}
        best_p99 = min(p99_vals.values()) if p99_vals else float('inf')

        p99_cells = []
        for a in allocators:
            if a not in alloc_map:
                p99_cells.append(f"{'N/A':>14}")
                continue
            v = alloc_map[a].get("p99_ns", 0)
            s = fmt_ns(v)
            if v == best_p99 and best_p99 < float('inf'):
                p99_cells.append(green(f"{s:>14}") + "*")
            else:
                p99_cells.append(f"{s:>14} ")
        print(f"  {'':34}  {'p99 lat':<10}  {'  '.join(p99_cells)}")

        # RSS row
        rss_cells = []
        for a in allocators:
            if a not in alloc_map:
                rss_cells.append(f"{'N/A':>14}")
                continue
            after  = alloc_map[a].get("rss_after", 0)
            before = alloc_map[a].get("rss_before", 0)
            delta  = after - before
            rss_cells.append(f"{fmt_bytes(delta):>14} ")
        print(f"  {'':34}  {'RSS Δ':<10}  {'  '.join(rss_cells)}")
        print()

# ── Bar chart ─────────────────────────────────────────────────────────────────
def print_bar_chart(by_scenario, allocators):
    print(bold("\n── Throughput bar chart (ops/sec, higher=better) ─────────────────────\n"))

    for scenario, alloc_map in sorted(by_scenario.items()):
        ops_vals = {a: alloc_map[a].get("ops_per_sec", 0) for a in allocators if a in alloc_map}
        if not ops_vals:
            continue
        max_ops = max(ops_vals.values())
        print(f"  {cyan(scenario)}")
        for a in sorted(ops_vals, key=lambda x: ops_vals[x], reverse=True):
            v = ops_vals[a]
            b = bar(v, max_ops)
            marker = green("★") if v == max_ops else " "
            print(f"    {a:>12}  {b}  {fmt_ops(v)} {marker}")
        print()

# ── Scoring / ranking ─────────────────────────────────────────────────────────
def print_ranking(by_scenario, allocators):
    print(bold("\n── Overall ranking (wins per allocator) ─────────────────────────────\n"))

    wins = defaultdict(int)
    total = 0
    for scenario, alloc_map in by_scenario.items():
        ops_vals = {a: alloc_map[a].get("ops_per_sec", 0) for a in allocators if a in alloc_map}
        if not ops_vals:
            continue
        best = max(ops_vals, key=lambda x: ops_vals[x])
        wins[best] += 1
        total += 1

    ranked = sorted(allocators, key=lambda a: wins[a], reverse=True)
    for i, a in enumerate(ranked):
        w = wins[a]
        pct = 100 * w / total if total > 0 else 0
        medal = ["🥇","🥈","🥉","  ","  "][min(i, 4)]
        b = bar(w, total, width=20)
        print(f"  {medal} {a:<14} {b}  {w}/{total} scenarios ({pct:.0f}%)")
    print()

# ── CSV export ───────────────────────────────────────────────────────────────
CSV_HEADER = "allocator,scenario,ops_per_sec,min_ns,p50_ns,p90_ns,p99_ns,max_ns,mean_ns,stddev_ns,rss_before,rss_after"

def export_csv(rows, path):
    with open(path, "w") as f:
        f.write(CSV_HEADER + "\n")
        for r in rows:
            f.write(
                "{},{},{:.2f},{},{},{},{},{},{:.2f},{:.2f},{},{}\n".format(
                    r.get("allocator", ""),
                    r.get("scenario", ""),
                    r.get("ops_per_sec", 0),
                    r.get("min_ns", 0),
                    r.get("p50_ns", 0),
                    r.get("p90_ns", 0),
                    r.get("p99_ns", 0),
                    r.get("max_ns", 0),
                    r.get("mean_ns", 0),
                    r.get("stddev_ns", 0),
                    r.get("rss_before", 0),
                    r.get("rss_after", 0),
                )
            )
    print(f"  CSV written to: {path}")

# ── Markdown export ───────────────────────────────────────────────────────────
def export_markdown(by_scenario, allocators, path):
    with open(path, "w") as f:
        f.write("# palloc Benchmark Results\n\n")
        f.write(f"| {'Scenario':<34} | {'Metric':<10} | " + " | ".join(f"{a:>14}" for a in allocators) + " |\n")
        f.write("|" + "-"*36 + "|" + "-"*12 + "|" + "|".join("-"*16 for _ in allocators) + "|\n")
        for scenario, alloc_map in sorted(by_scenario.items()):
            for metric_key, metric_name in [("ops_per_sec", "ops/sec"), ("p99_ns", "p99 ns"), ("rss_after", "RSS")]:
                row = f"| {scenario:<34} | {metric_name:<10} |"
                for a in allocators:
                    v = alloc_map.get(a, {}).get(metric_key, None)
                    if v is None:         cell = "N/A"
                    elif metric_key == "ops_per_sec": cell = fmt_ops(v)
                    elif metric_key == "rss_after":   cell = fmt_bytes(v)
                    else:                              cell = fmt_ns(v)
                    row += f" {cell:>14} |"
                f.write(row + "\n")
    print(f"  Markdown table written to: {path}")

# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "results_all.json"
    if not os.path.exists(path):
        print(f"No results file found: {path}", file=sys.stderr)
        print("Run 'bash run_bench.sh' to generate results.", file=sys.stderr)
        sys.exit(1)

    rows = load(path)
    if not rows:
        print("No result rows found in file.", file=sys.stderr)
        sys.exit(1)

    allocators = sorted(set(r.get("allocator", "?") for r in rows))
    by_scenario = group_by_scenario(rows)

    print(bold(f"\n{'═'*80}"))
    print(bold(f"  palloc Benchmark Report"))
    print(bold(f"  Allocators: {', '.join(allocators)}"))
    print(bold(f"  Scenarios:  {len(by_scenario)}"))
    print(bold(f"{'═'*80}"))

    print_table(by_scenario, allocators)
    print_bar_chart(by_scenario, allocators)
    print_ranking(by_scenario, allocators)

    out_dir = os.path.dirname(path)
    md_path = os.path.join(out_dir, "results_report.md")
    csv_path = os.path.join(out_dir, "results.csv")
    export_markdown(by_scenario, allocators, md_path)
    export_csv(rows, csv_path)

    print(bold("\n  * = best in category\n"))

if __name__ == "__main__":
    main()
