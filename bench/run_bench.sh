#!/usr/bin/env bash
# run_bench.sh — Build and run all available allocator benchmarks,
#               merge results into results.json, print final comparison table.
#
# Usage:
#   cd /path/to/palloc/bench
#   bash run_bench.sh [--threads N] [--iter N] [--output FORMAT] [scenario...]
#
# This script MUST be run from inside the bench build directory, OR from the
# bench source directory (it will auto-create/enter 'build/').

set -euo pipefail

# ── Defaults ──────────────────────────────────────────────────────────────
THREADS=${BENCH_THREADS:-$(nproc)}
ITER=${BENCH_ITER:-2000000}
WARMUP=${BENCH_WARMUP:-50000}
OUTPUT_FMT=${BENCH_OUTPUT:-table}
EXTRA_ARGS=()

# ── Arg parsing ───────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
  case "$1" in
    --threads=*) THREADS="${1#--threads=}" ;;
    --iter=*)    ITER="${1#--iter=}" ;;
    --warmup=*)  WARMUP="${1#--warmup=}" ;;
    --output=*)  OUTPUT_FMT="${1#--output=}" ;;
    *)           EXTRA_ARGS+=("$1") ;;
  esac
  shift
done

COMMON_ARGS="--threads=${THREADS} --iter=${ITER} --warmup=${WARMUP}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ── Locate build directory ─────────────────────────────────────────────────
BUILD_DIR=""
if [[ -f "bench-palloc" ]]; then
  BUILD_DIR="$(pwd)"
elif [[ -d "${SCRIPT_DIR}/build" ]]; then
  BUILD_DIR="${SCRIPT_DIR}/build"
else
  echo ">> No build directory found. Building now..."
  BUILD_DIR="${SCRIPT_DIR}/build"
  cmake -B "${BUILD_DIR}" -S "${SCRIPT_DIR}" -DCMAKE_BUILD_TYPE=Release -Wno-dev
  cmake --build "${BUILD_DIR}" -j"$(nproc)"
fi

# ── Helper: run one allocator benchmark ───────────────────────────────────
RESULTS_DIR="${BUILD_DIR}"
MERGE_FILE="${RESULTS_DIR}/results_all.json"

run_alloc() {
  local binary="$1"
  local name="$2"
  local extra_env="${3:-}"

  if [[ ! -x "${BUILD_DIR}/${binary}" ]]; then
    echo "  [SKIP] ${binary} not built"
    return
  fi

  local out_json="${RESULTS_DIR}/results_${name}.json"
  echo ""
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo " Running: ${binary}  (${THREADS} threads, ${ITER} iter)"
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

  env ${extra_env} "${BUILD_DIR}/${binary}" \
    ${COMMON_ARGS} \
    --output=json \
    --output-file="${out_json}" \
    "${EXTRA_ARGS[@]}" || true

  # Also print human table to terminal
  env ${extra_env} "${BUILD_DIR}/${binary}" \
    ${COMMON_ARGS} \
    --output=table \
    "${EXTRA_ARGS[@]}" || true

  echo "  Results written to: ${out_json}"
}

# ── Run all allocators ─────────────────────────────────────────────────────
run_alloc bench-palloc    palloc
run_alloc bench-mimalloc  mimalloc
run_alloc bench-jemalloc  jemalloc
run_alloc bench-tcmalloc  tcmalloc
run_alloc bench-system    system

# ── Merge JSON files ───────────────────────────────────────────────────────
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo " Merging JSON results → ${MERGE_FILE}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

export BUILD_DIR
python3 - <<'PYEOF'
import json, glob, sys, os

build = os.environ.get("BUILD_DIR", ".")
files = glob.glob(os.path.join(build, "results_*.json"))
merged = []
for f in sorted(files):
    try:
        data = json.load(open(f))
        merged.append(data)
    except Exception as e:
        print(f"  Warning: skipping {f}: {e}", file=sys.stderr)

with open(os.path.join(build, "results_all.json"), "w") as out:
    json.dump(merged, out, indent=2)

print(f"  Merged {len(merged)} allocator results into results_all.json")
PYEOF

export BUILD_DIR
python3 "${SCRIPT_DIR}/bench_report.py" "${BUILD_DIR}/results_all.json"

echo ""
echo "Done. Full JSON: ${MERGE_FILE}"
