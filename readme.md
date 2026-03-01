# palloc

<img align="left" width="100" height="100" src="./media/logo.png"/>

palloc is a general purpose allocator with excellent performance characteristics. Originally developed as `mimalloc`, it is designed to be a drop-in replacement for the standard `malloc` and can be used in projects without code changes.

## Key Features

- **Small and Consistent**: The library is about 10k LOC using simple and consistent data structures.
- **High Performance**: Outperforms many leading allocators in various benchmarks with low internal fragmentation.
- **Free List Sharding**: Uses multiple free lists per page to reduce contention and increase locality.
- **Secure**: Can be built in secure mode with guard pages and other mitigations.
- **First-Class Heaps**: Efficiently create and use multiple heaps across different threads.
- **Broad Compatibility**: Supports Windows, macOS, Linux, WASM, and various BSDs.

## Quick Start

### Building on Linux/macOS

```bash
mkdir -p build
cd build
cmake ..
make
sudo make install
```

### Usage

Link with `libpalloc` and include `<palloc.h>` (or use `LD_PRELOAD` for dynamic overriding).

```bash
# Example compilation
gcc -o myprogram myfile.c -lpalloc
```

For more detailed documentation, please refer to the `doc/` directory or visit the [project documentation](https://microsoft.github.io/palloc).

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
