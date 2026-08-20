# palloc — High-Performance, Low-Latency General-Purpose Memory Allocator

<img src="./media/logo.png" alt="palloc memory allocator logo"/>

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](#quick-start)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20WASM%20%7C%20BSD-lightgrey.svg)](#broad-platform-compatibility)

**palloc** is an ultra-fast, lightweight, and thread-safe general-purpose memory allocator designed for high throughput, low latency, and minimal memory fragmentation. Originally evolved from `mimalloc`, palloc delivers a zero-code-change, drop-in replacement for standard `malloc`, `calloc`, `realloc`, and `free` across multi-threaded applications, edge systems, database engines, and resource-constrained environments.

---

## Key Features

- **Blazing Fast Throughput**: Outperforms standard `glibc malloc`, `tcmalloc`, and other system allocators across diverse multi-threaded and allocation-heavy workloads.
- **Low Internal Fragmentation**: Utilizes deterministic page layouts and fine-grained size classes to minimize memory bloat.
- **Free List Sharding**: Employs sharded free lists per memory page to drastically reduce cross-thread CPU cache bouncing and lock contention.
- **Drop-in `malloc` Replacement**: Full runtime override support via `LD_PRELOAD`, `DYLD_INSERT_LIBRARIES`, or Windows redirect DLL without recompiling legacy binaries.
- **Compact & Deterministic (~10k LOC)**: Simple, maintainable, and cache-friendly internal data structures designed for modern CPU architectures.
- **First-Class Dynamic Heaps**: Supports multi-heap instantiation and thread-local allocation pools with instant bulk reclamation.
- **Hardened Security Modes**: Optional compile-time security mitigations, including guard pages and overflow padding.
- **Broad Platform Compatibility**: First-class support for Linux, macOS, Windows (MSVC/MinGW), WebAssembly (WASM), and modern BSD systems.

---

## Benchmark Results

palloc includes an automated benchmarking suite evaluating throughput, cache locality, and fragmentation against **glibc**, **mimalloc**, and **tcmalloc** across 13 execution scenarios (single-threaded bursts, multi-threaded contention, mixed size classes, realloc scaling, and fragmented retention).

To reproduce the benchmark suite:

```bash
cd bench
bash run_bench.sh                    # Full suite (or use --iter=50000 for rapid profiling)
# Generates: build/results.csv, build/results_all.json, build/results_report.md

```

---

## Quick Start

### 1. Building on Linux / macOS

Standard CMake build:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
sudo cmake --install .

```

Fast build using Ninja:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build

```

### 2. Building on Windows

**Using Visual Studio (MSVC):**

1. Open a **Developer Command Prompt for VS** (ensure `cl` and `cmake` are present in `PATH`).
2. Run:

```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
cmake --install build --prefix C:\palloc-install

```

*(For 32-bit use `-A Win32`; for ARM64 use `-A ARM64`)*

**Using MinGW (GCC on Windows):**

```cmd
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build
cmake --install build

```

**Windows Runtime Malloc Override:**

When building the shared library (`.dll`), palloc utilizes a redirect DLL to override standard CRT allocations. If `bin/palloc-redirect.dll` is not present, it will be built from source automatically. To build the static/shared library without runtime override hooking, disable the redirect:

```cmd
cmake -B build -DPA_WIN_REDIRECT=OFF ...

```

---

## Build Configuration Options

| Option | Default | Description |
| --- | --- | --- |
| `CMAKE_BUILD_TYPE` | `Release` | Target configuration: `Release`, `Debug`, `RelWithDebInfo` |
| `CMAKE_INSTALL_PREFIX` | System | Destination directory (e.g., `/usr/local`, `C:\palloc`) |
| `PA_BUILD_SHARED` | `ON` | Build shared library (`.so`, `.dylib`, `.dll`) |
| `PA_BUILD_STATIC` | `ON` | Build static library archive (`.a`, `.lib`) |
| `PA_BUILD_TESTS` | `ON` | Build test and validation suites |
| `PA_OVERRIDE` | `ON` | Enable global `malloc`/`free` symbol interception |
| `PA_WIN_REDIRECT` | `ON` | Enable Windows DLL dynamic hook redirect layer |
| `PA_OPT_ARCH` | `OFF` (x64), `ON` (ARM64) | Architecture-specific assembly/atomic performance paths |
| `PA_PADDING` | `OFF` | Buffer padding to detect out-of-bounds heap writes |
| `PA_SECURE` | `OFF` | Enable full security hardening and guard page placement |
| `PA_DEBUG` / `PA_DEBUG_FULL` | `OFF` | Enable assertions, invariant checks, and leak tracing |

### Maximum Throughput Preset

To build with maximum throughput and lowest allocation latency (disabling debug checks and padding while activating architectural vectorization):

```bash
cmake --preset release-maxperf && cmake --build build

```

---

## Usage

### Linking Directly

```c
#include <palloc.h>
#include <stdio.h>

int main(void) {
    void* ptr = pa_malloc(1024);
    // User allocation logic
    pa_free(ptr);
    return 0;
}

```

**Compile and link:**

```bash
# Linux / macOS
gcc -O3 -o myprogram myfile.c -lpalloc

# Windows (MSVC)
cl myfile.c /I path\to\palloc-install\include path\to\palloc-install\lib\palloc.lib

```

### Transparent Dynamic Library Override

Inject palloc into any precompiled application without code modification or recompilation:

```bash
# Linux
LD_PRELOAD=/usr/local/lib/libpalloc.so ./my_application

# macOS
DYLD_INSERT_LIBRARIES=/usr/local/lib/libpalloc.dylib ./my_application

```

On Windows, place `palloc.dll` and `palloc-redirect.dll` into the same directory as the target executable.

---

## License

palloc is licensed under the [MIT License](https://www.google.com/search?q=LICENSE).
