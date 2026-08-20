# Contributing to palloc

Thank you for your interest in contributing to **palloc**! As a low-level, high-performance memory allocator, reliability, deterministic behavior, and extreme performance are critical.

Please review the following guidelines before submitting issues or pull requests.

---

## Code of Conduct

Be respectful, collaborative, and constructive. We focus on engineering rigor, objective data, and clear communication.

---

## How to Contribute

### 1. Reporting Bugs & Regressions
Before filing a bug report:
- Ensure the issue reproduces with the latest `main` branch.
- Test under both standard and debug/sanitizer builds (`-DCMAKE_BUILD_TYPE=Debug` or with `-DPA_DEBUG=ON`).
- Include:
  - **Environment details:** OS, CPU architecture (x86_64, ARM64, etc.), compiler and version (GCC, Clang, MSVC).
  - **Build configuration:** CMake flags used.
  - **Minimal Reproducible Example (MRE):** A self-contained C/C++ snippet demonstrating the crash, memory leak, or corruption.
  - **Backtrace / ASan Output:** Full stack trace from GDB, LLDB, or AddressSanitizer.

### 2. Suggesting Performance Optimizations
- Performance improvements must be backed by reproducible numbers.
- Run the internal benchmark suite before and after your changes:
  ```bash
  cd bench
  bash run_bench.sh

```

* Attach the generated reports (`build/results_report.md` or JSON comparisons) to your proposal.

---

## Development & Build Workflow

### Prerequisites

* **C/C++ Compiler:** GCC 11+, Clang 14+, or MSVC (Visual Studio 2022+)
* **Build System:** CMake 3.20+ and Ninja (recommended)

### Setting Up Local Environment

```bash
# Clone the repository with submodules
git clone --recursive [https://github.com/pomagrenate/palloc.git](https://github.com/pomagrenate/palloc.git)
cd palloc

# Configure debug build with tests enabled
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPA_BUILD_TESTS=ON

# Build the project and test suite
cmake --build build

# Run unit tests
ctest --test-dir build --output-on-failure

```

---

## Coding Standards

Because `palloc` operates as a core memory manager and malloc override, standard runtime assumptions do not always apply:

1. **Pure C / C++ Compatibility:** Core allocator logic must remain standard-compliant C with minimal external dependencies.
2. **No Recursive Allocations:** Never call standard library functions that internally invoke `malloc`/`free` inside allocation paths (e.g., avoid standard I/O, dynamic allocations, or unmanaged OS hooks).
3. **Thread Safety & Atomics:** Use fine-grained atomic primitives where applicable. Avoid heavy locking mechanisms in fast paths.
4. **Zero Unintended Overhead:** Keep fast-path functions inlined and free of conditional branches whenever possible.
5. **Code Style:**
* Consistent 2-space or 4-space indentation (matching surrounding source files).
* Descriptive variable names for complex bit-manipulation routines.
* Clear inline comments explaining platform-specific assembly, barriers, or memory orderings (`memory_order_acquire`, `memory_order_release`).



---

## Pull Request Process

1. **Branch Naming:** Use descriptive branch names (`feat/arm64-atomics`, `fix/page-alignment-leak`, `bench/realloc-throughput`).
2. **Commit Hygiene:** Write clear, imperative commit messages (e.g., `fix: resolve race condition in thread-local page reclamation`).
3. **Pass CI & Tests:** Ensure all tests pass locally across Debug and Release configurations.
4. **Benchmark Verification:** If changing allocation paths, provide benchmark comparisons against `main`.
5. **Review:** A maintainer will review your code for thread safety, edge-case allocations, and portability before merging.

---

## License

By contributing to **palloc**, you agree that your contributions will be licensed under the project's [MIT License](https://www.google.com/search?q=LICENSE).