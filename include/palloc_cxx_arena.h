/* ----------------------------------------------------------------------------
 * palloc_cxx_arena.h — C++20 STL-compatible allocator for Pomai Arena API
 *
 * Bridges the C-core Arena API (p_arena_* from arena_pomai.c) for use with
 * standard C++ containers. Deallocate is a no-op; memory is reclaimed in bulk
 * via reset() at the end of the query lifecycle.
 *
 * Use with: std::vector<T, PomaiArenaAllocator<T>>, etc.
 * ------------------------------------------------------------------------- */
#pragma once

#ifndef PALLOC_CXX_ARENA_H
#define PALLOC_CXX_ARENA_H

#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>

#include "palloc/arena_pomai.h"

namespace palloc {

/**
 * C++20 STL-compatible allocator that backs allocations with the palloc
 * bump-pointer arena (O(1) alloc, no-op dealloc, O(1) bulk reset).
 *
 * Requirement: deallocate() does nothing; free memory only via reset() or
 * arena destruction.
 */
template <class T>
struct PomaiArenaAllocator {
  // ————— Required by C++20 std::allocator_traits —————
  using value_type      = T;
  using size_type       = std::size_t;
  using difference_type = std::ptrdiff_t;
  using propagate_on_container_copy_assignment = std::false_type;
  using propagate_on_container_move_assignment  = std::true_type;
  using propagate_on_container_swap             = std::false_type;
  using is_always_equal                         = std::false_type;

  template <class U>
  struct rebind {
    using other = PomaiArenaAllocator<U>;
  };

  // ————— Arena handle (opaque from C API) —————
  void* arena_{nullptr};

  PomaiArenaAllocator() noexcept = default;

  explicit PomaiArenaAllocator(void* arena) noexcept : arena_(arena) {}

  /** Create and own an arena of the given size (bytes). Calls p_arena_create. */
  explicit PomaiArenaAllocator(size_type arena_size_bytes) noexcept
    : arena_(::p_arena_create(arena_size_bytes)) {}

  template <class U>
  PomaiArenaAllocator(const PomaiArenaAllocator<U>& other) noexcept
    : arena_(other.arena_) {}

  PomaiArenaAllocator(const PomaiArenaAllocator&) noexcept = default;
  PomaiArenaAllocator(PomaiArenaAllocator&&) noexcept = default;
  PomaiArenaAllocator& operator=(const PomaiArenaAllocator&) noexcept = default;
  PomaiArenaAllocator& operator=(PomaiArenaAllocator&&) noexcept = default;

  [[nodiscard]] T* allocate(size_type n) {
    if (n == 0)
      return nullptr;
    if (arena_ == nullptr)
      throw std::bad_alloc{};
    if (n > max_size())
      throw std::bad_alloc{};
    void* p = ::p_arena_alloc(arena_, n * sizeof(T));
    if (p == nullptr)
      throw std::bad_alloc{};
    return static_cast<T*>(p);
  }

  /** No-op: arena does not support per-block free; use reset() for bulk reclaim. */
  void deallocate(T* /* p */, size_type /* n */) noexcept {
    // Intentionally empty — memory is freed in bulk via p_arena_reset().
  }

  /** Bulk reclaim all arena memory (O(1)). Call at end of query lifecycle. */
  void reset() noexcept {
    if (arena_ != nullptr)
      ::p_arena_reset(arena_);
  }

  [[nodiscard]] size_type max_size() const noexcept {
    return std::numeric_limits<size_type>::max() / sizeof(T);
  }

  [[nodiscard]] void* arena() const noexcept { return arena_; }

  template <class U, class V>
  friend bool operator==(const PomaiArenaAllocator<U>& a, const PomaiArenaAllocator<V>& b) noexcept {
    return a.arena_ == b.arena_;
  }

  template <class U, class V>
  friend bool operator!=(const PomaiArenaAllocator<U>& a, const PomaiArenaAllocator<V>& b) noexcept {
    return !(a == b);
  }
};

} // namespace palloc

#endif // PALLOC_CXX_ARENA_H
