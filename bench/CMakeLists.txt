cmake_minimum_required(VERSION 3.18)
project(palloc-bench C)

set(CMAKE_C_STANDARD 11)

# ---------------------------------------------------------------------------
# Build type
# ---------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ---------------------------------------------------------------------------
# Locate palloc (sibling build)
# ---------------------------------------------------------------------------
set(PALLOC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.."
    CACHE PATH "Root of the palloc source tree")

# Search paths: Unix build dirs and Windows multi-config (Release/Debug)
set(PALLOC_LIB_HINTS
  "${PALLOC_ROOT}/out/release"
  "${PALLOC_ROOT}/build"
)
if(WIN32)
  list(APPEND PALLOC_LIB_HINTS
    "${PALLOC_ROOT}/build/Release" "${PALLOC_ROOT}/build/Debug"
    "${PALLOC_ROOT}/out/release/Release" "${PALLOC_ROOT}/out/release/Debug")
endif()

find_library(PALLOC_LIB
  NAMES palloc palloc.dll
  HINTS ${PALLOC_LIB_HINTS}
  DOC   "palloc shared or static library")
find_path(PALLOC_INCLUDE
  NAMES palloc.h
  HINTS "${PALLOC_ROOT}/include"
  DOC   "palloc include directory")

if(NOT PALLOC_LIB OR NOT PALLOC_INCLUDE)
  message(FATAL_ERROR "Could not find palloc library/headers. "
    "Build palloc first (e.g. 'cmake -B build && cmake --build build' in the repo root) "
    "or set PALLOC_ROOT.")
endif()
message(STATUS "palloc: ${PALLOC_LIB} (include: ${PALLOC_INCLUDE})")

# ---------------------------------------------------------------------------
# Common compile options (platform-specific)
# ---------------------------------------------------------------------------
set(BENCH_COMMON_SOURCES bench_scenarios.c bench_main.c)
set(BENCH_COMMON_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR})

# Benchmark suite uses pthread and POSIX timing; build on Unix and MinGW, skip on MSVC
set(BENCH_BUILD_ENABLED ON)
if(WIN32 AND MSVC)
  set(BENCH_BUILD_ENABLED OFF)
  message(STATUS "Benchmark suite skipped on Windows MSVC (requires pthread). Use MinGW to run benchmarks on Windows.")
endif()

if(BENCH_BUILD_ENABLED)
  # Bench uses pthread and POSIX APIs; use Unix-style flags (GCC/Clang/MinGW)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR MINGW)
    set(BENCH_COMMON_FLAGS -O3 -g -Wall -Wextra -march=native -pthread)
    set(BENCH_LINK_LIBS m pthread)
  else()
    set(BENCH_COMMON_FLAGS -O3 -g -Wall -Wextra)
    set(BENCH_LINK_LIBS m pthread)
  endif()
endif()

# ---------------------------------------------------------------------------
# Helper macro to register a benchmark target
# ---------------------------------------------------------------------------
macro(add_bench TARGET DEFINE LINK_LIBS)
  if(NOT BENCH_BUILD_ENABLED)
    return()
  endif()
  add_executable(${TARGET} ${BENCH_COMMON_SOURCES})
  target_include_directories(${TARGET} PRIVATE ${BENCH_COMMON_INCLUDES} ${PALLOC_INCLUDE})
  target_compile_options(${TARGET} PRIVATE ${BENCH_COMMON_FLAGS})
  target_compile_definitions(${TARGET} PRIVATE ${DEFINE})
  target_link_libraries(${TARGET} PRIVATE ${LINK_LIBS} ${BENCH_LINK_LIBS})
  message(STATUS "Benchmark target: ${TARGET}  (${DEFINE})")
endmacro()

# ---------------------------------------------------------------------------
# palloc target (always available)
# ---------------------------------------------------------------------------
add_bench(bench-palloc "BENCH_USE_PALLOC" "${PALLOC_LIB}")

# ---------------------------------------------------------------------------
# system malloc baseline
# ---------------------------------------------------------------------------
add_bench(bench-system "" "")

# ---------------------------------------------------------------------------
# mimalloc (optional)
# ---------------------------------------------------------------------------
find_library(MIMALLOC_LIB  NAMES mimalloc  DOC "mimalloc shared library")
find_path(MIMALLOC_INCLUDE NAMES mimalloc.h PATH_SUFFIXES mimalloc DOC "mimalloc include dir")
if(MIMALLOC_LIB AND MIMALLOC_INCLUDE)
  add_bench(bench-mimalloc "BENCH_USE_MIMALLOC" "${MIMALLOC_LIB}")
  target_include_directories(bench-mimalloc PRIVATE ${MIMALLOC_INCLUDE})
else()
  message(WARNING "mimalloc not found — skipping bench-mimalloc. "
    "Install: sudo apt-get install libmimalloc-dev")
endif()

# ---------------------------------------------------------------------------
# jemalloc (optional)
# ---------------------------------------------------------------------------
find_library(JEMALLOC_LIB  NAMES jemalloc  DOC "jemalloc shared library")
find_path(JEMALLOC_INCLUDE NAMES jemalloc/jemalloc.h DOC "jemalloc include dir")
if(JEMALLOC_LIB AND JEMALLOC_INCLUDE)
  add_bench(bench-jemalloc "BENCH_USE_JEMALLOC" "${JEMALLOC_LIB}")
  target_include_directories(bench-jemalloc PRIVATE ${JEMALLOC_INCLUDE})
else()
  message(WARNING "jemalloc not found — skipping bench-jemalloc. "
    "Install: sudo apt-get install libjemalloc-dev")
endif()

# ---------------------------------------------------------------------------
# tcmalloc (optional — uses LD_PRELOAD override + link)
# ---------------------------------------------------------------------------
find_library(TCMALLOC_LIB
  NAMES tcmalloc_minimal tcmalloc
  DOC   "tcmalloc shared library")
if(TCMALLOC_LIB)
  add_bench(bench-tcmalloc "BENCH_USE_TCMALLOC" "${TCMALLOC_LIB}")
else()
  message(WARNING "tcmalloc not found — skipping bench-tcmalloc. "
    "Install: sudo apt-get install libgoogle-perftools-dev")
endif()

# ---------------------------------------------------------------------------
# Install convenience copies
# ---------------------------------------------------------------------------
if(BENCH_BUILD_ENABLED)
  install(TARGETS bench-palloc bench-system DESTINATION bin)
endif()

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
message(STATUS "")
if(BENCH_BUILD_ENABLED)
  message(STATUS "Benchmark targets configured:")
  get_property(_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
  foreach(_t ${_targets})
    if(_t MATCHES "^bench-")
      message(STATUS "  cmake --build . --target ${_t}")
    endif()
  endforeach()
  message(STATUS "")
  message(STATUS "Quick start (Linux/macOS/MinGW):")
  if(UNIX AND NOT MINGW)
    message(STATUS "  cmake -B build -S . && cmake --build build -j$(nproc)")
    message(STATUS "  cd build && bash ../run_bench.sh")
  else()
    message(STATUS "  cmake -B build -S . && cmake --build build")
    message(STATUS "  cd build && run_bench.sh (or run bench-palloc.exe manually)")
  endif()
else()
  message(STATUS "Benchmark suite not built on this platform.")
endif()
message(STATUS "")
