# Benchmarks

## Dual-mode (core API): USER and KERNEL

When palloc is built with **PALLOC_BUILD_MODE=USER** or **KERNEL**, the root build adds in-tree benchmarks that use only the core API (`pa_core_malloc`/`pa_core_free`, `palloc_vector_*`). **Run all commands from the repository root** so that `./build/...` paths resolve.

| Mode   | Target             | How to run |
|--------|--------------------|------------|
| **USER**   | `bench-user-core`   | Build, then run `./build/bench-user-core` from repo root. Reports throughput (Mop/s, Kop/s). |
| **KERNEL** | `bench-kernel-core` | Default: uses fixed RAM at 0x40000000 — **do not run on host** (segfault). Run under PoOS or QEMU. |
| **KERNEL (host-safe)** | `bench-kernel-core` | Build with `-DPALLOC_KERNEL_RUN_ON_HOST=ON`; uses mmap so you can run `./build/bench-kernel-core` on host. |

### Build and run (USER)

```bash
cmake -B build -DPALLOC_BUILD_MODE=USER
cmake --build build --target bench-user-core
./build/bench-user-core
```

### Build and run (KERNEL) — on host

To run the kernel benchmark on your machine (e.g. CI or dev), use the host-safe backend:

```bash
cmake -B build -DPALLOC_BUILD_MODE=KERNEL -DPALLOC_KERNEL_RUN_ON_HOST=ON
cmake --build build --target bench-kernel-core
./build/bench-kernel-core
```

### Build (KERNEL) — for PoOS/QEMU

For real kernel target (no libc, address 0x40000000). Do **not** run the resulting binary on host — it will segfault.

```bash
cmake -B build -DPALLOC_BUILD_MODE=KERNEL
cmake --build build --target bench-kernel-core
# Run ./build/bench-kernel-core under PoOS or QEMU only
```

---

## Legacy (full API): standalone bench suite

The **bench/** directory is also a standalone CMake project. It finds a pre-built palloc (legacy, prim-based) and builds the full benchmark suite (threaded scenarios, vector arena, etc.). Use it to compare palloc with system malloc, mimalloc, jemalloc, tcmalloc.

See the root [README](../README.md) and run from the bench directory:

```bash
cd bench
cmake -B build -S .
cmake --build build
cd build && bash ../run_bench.sh
```
