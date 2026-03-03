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

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
