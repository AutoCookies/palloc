# Test suite

All tests live in this **test/** directory (the former `tests/` folder was merged here; there is a single test tree).

## Building and running (in-tree)

From the repo root, tests are built and run when `PA_BUILD_TESTS=ON` (default):

```bash
cmake -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test executables (built by root CMakeLists.txt)

| Source | Purpose |
|-------|--------|
| **test-api.c** | API surface: malloc/calloc/realloc/free, aligned, heaps, STL allocator, edge cases. |
| **test-api-fill.c** | Fills allocations to detect overwrites (use with debug/valgrind). |
| **test-stress.c** | Multi-threaded stress; also built as `test-stress-dynamic` (LD_PRELOAD override). |
| **test-arena-pomai.c** | Arena Pomai API: create, alloc, reset, destroy (bump allocator). |
| **test-basic.c** | Version-agnostic basic API check (no fixed version asserted); malloc/free/calloc/realloc, aligned. |
| **test-wrong.c** | Intentional misuse / memory errors (used by standalone `test/CMakeLists.txt` when testing an install). |

## Override / install verification (standalone)

The **test/CMakeLists.txt** in this folder is a separate project: it expects palloc to be **installed** (`find_package(palloc)`). It builds programs that test overriding (dynamic/static) and `test-wrong`. Use it to verify a local install:

```bash
# After: cmake --install build --prefix /path/to/install
cd test && cmake -B build -DCMAKE_PREFIX_PATH=/path/to/install && cmake --build test/build
```

## Testing strategy

- **Internal checks:** Debug builds use extensive invariant checking (e.g. `page_is_valid` in `page.c`). Use `-DPA_DEBUG_FULL=ON` for full checking.
- **Benchmarks:** Run the **bench/** suite with full invariant checking to stress the allocator; see [bench/](../bench/) and the main [README](../README.md).

[bench]: https://github.com/daanx/palloc-bench
