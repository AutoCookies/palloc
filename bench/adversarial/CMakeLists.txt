cmake_minimum_required(VERSION 3.18)
project(adversarial_bench C)

include(CheckIncludeFiles)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=native")

# Find palloc
find_package(palloc CONFIG REQUIRED)

# Adversarial benchmark executable
add_executable(adversarial_bench
    adversarial_main.c
    adversarial_bench.c
)

# Link with palloc and math library
target_link_libraries(adversarial_bench palloc m)

# Conditional allocator support
option(ADVERSARIAL_USE_MIMALLOC "Build with mimalloc support" OFF)
option(ADVERSARIAL_USE_JEMALLOC "Build with jemalloc support" OFF)
option(ADVERSARIAL_USE_TCMALLOC "Build with tcmalloc support" OFF)

if(ADVERSARIAL_USE_MIMALLOC)
    find_package(mimalloc REQUIRED)
    target_link_libraries(adversarial_bench mimalloc::mimalloc)
    target_compile_definitions(adversarial_bench PRIVATE BENCH_USE_MIMALLOC)
endif()

if(ADVERSARIAL_USE_JEMALLOC)
    find_package(jemalloc REQUIRED)
    target_link_libraries(adversarial_bench jemalloc::jemalloc)
    target_compile_definitions(adversarial_bench PRIVATE BENCH_USE_JEMALLOC)
endif()

if(ADVERSARIAL_USE_TCMALLOC)
    find_package(tcmalloc REQUIRED)
    target_link_libraries(adversarial_bench tcmalloc::tcmalloc)
    target_compile_definitions(adversarial_bench PRIVATE BENCH_USE_TCMALLOC)
endif()

# Build palloc version by default
target_compile_definitions(adversarial_bench PRIVATE BENCH_USE_PALLOC)

# AVX/AVX-512 support
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    check_include_files(immintrin.h HAVE_IMMINTRIN_H)
    if(HAVE_IMMINTRIN_H)
        target_compile_definitions(adversarial_bench PRIVATE HAVE_IMMINTRIN_H)
    endif()
endif()

# Install
install(TARGETS adversarial_bench DESTINATION bin)