/* ----------------------------------------------------------------------------
Copyright (c) 2018-2021, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

#include "palloc.h"
#include "palloc/internal.h"
#include "palloc/prim.h"  // pa_prim_get_default_heap

#include <string.h>     // memset

// ------------------------------------------------------
// Aligned Allocation
// ------------------------------------------------------

static bool pa_malloc_is_naturally_aligned( size_t size, size_t alignment ) {
  // objects up to `PA_MAX_ALIGN_GUARANTEE` are allocated aligned to their size (see `segment.c:_pa_segment_page_start`).
  pa_assert_internal(_pa_is_power_of_two(alignment) && (alignment > 0));
  if (alignment > size) return false;
  if (alignment <= PA_MAX_ALIGN_SIZE) return true;
  const size_t bsize = pa_good_size(size);
  return (bsize <= PA_MAX_ALIGN_GUARANTEE && (bsize & (alignment-1)) == 0);
}

#if PA_GUARDED
static pa_decl_restrict void* pa_heap_malloc_guarded_aligned(pa_heap_t* heap, size_t size, size_t alignment, bool zero) pa_attr_noexcept {
  // use over allocation for guarded blocksl
  pa_assert_internal(alignment > 0 && alignment < PA_BLOCK_ALIGNMENT_MAX);
  const size_t oversize = size + alignment - 1;
  void* base = _pa_heap_malloc_guarded(heap, oversize, zero);
  void* p = pa_align_up_ptr(base, alignment);
  pa_track_align(base, p, (uint8_t*)p - (uint8_t*)base, size);
  pa_assert_internal(pa_usable_size(p) >= size);
  pa_assert_internal(_pa_is_aligned(p, alignment));
  return p;
}

static void* pa_heap_malloc_zero_no_guarded(pa_heap_t* heap, size_t size, bool zero, size_t* usable) {
  const size_t rate = heap->guarded_sample_rate;
  // only write if `rate!=0` so we don't write to the constant `_pa_heap_empty`
  if (rate != 0) { heap->guarded_sample_rate = 0; }
  void* p = _pa_heap_malloc_zero_ex(heap, size, zero, 0, usable);
  if (rate != 0) { heap->guarded_sample_rate = rate; }
  return p;
}
#else
static void* pa_heap_malloc_zero_no_guarded(pa_heap_t* heap, size_t size, bool zero, size_t* usable) {
  return _pa_heap_malloc_zero_ex(heap, size, zero, 0, usable);
}
#endif

// Fallback aligned allocation that over-allocates -- split out for better codegen
static pa_decl_noinline void* pa_heap_malloc_zero_aligned_at_overalloc(pa_heap_t* const heap, const size_t size, const size_t alignment, const size_t offset, const bool zero, size_t* usable) pa_attr_noexcept
{
  pa_assert_internal(size <= (PA_MAX_ALLOC_SIZE - PA_PADDING_SIZE));
  pa_assert_internal(alignment != 0 && _pa_is_power_of_two(alignment));

  void* p;
  size_t oversize;
  if pa_unlikely(alignment > PA_BLOCK_ALIGNMENT_MAX) {
    // use OS allocation for very large alignment and allocate inside a huge page (dedicated segment with 1 page)
    // This can support alignments >= PA_SEGMENT_SIZE by ensuring the object can be aligned at a point in the
    // first (and single) page such that the segment info is `PA_SEGMENT_SIZE` bytes before it (so it can be found by aligning the pointer down)
    if pa_unlikely(offset != 0) {
      // todo: cannot support offset alignment for very large alignments yet
#if PA_DEBUG > 0
      _pa_error_message(EOVERFLOW, "aligned allocation with a very large alignment cannot be used with an alignment offset (size %zu, alignment %zu, offset %zu)\n", size, alignment, offset);
#endif
      return NULL;
    }
    oversize = (size <= PA_SMALL_SIZE_MAX ? PA_SMALL_SIZE_MAX + 1 /* ensure we use generic malloc path */ : size);
    // note: no guarded as alignment > 0
    p = _pa_heap_malloc_zero_ex(heap, oversize, false, alignment, usable); // the page block size should be large enough to align in the single huge page block
    // zero afterwards as only the area from the aligned_p may be committed!
    if (p == NULL) return NULL;
  }
  else {
    // otherwise over-allocate
    oversize = (size < PA_MAX_ALIGN_SIZE ? PA_MAX_ALIGN_SIZE : size) + alignment - 1;  // adjust for size <= 16; with size 0 and aligment 64k, we would allocate a 64k block and pointing just beyond that.
    p = pa_heap_malloc_zero_no_guarded(heap, oversize, zero, usable);
    if (p == NULL) return NULL;
  }
  pa_page_t* page = _pa_ptr_page(p);

  // .. and align within the allocation
  const uintptr_t align_mask = alignment - 1;  // for any x, `(x & align_mask) == (x % alignment)`
  const uintptr_t poffset = ((uintptr_t)p + offset) & align_mask;
  const uintptr_t adjust  = (poffset == 0 ? 0 : alignment - poffset);
  pa_assert_internal(adjust < alignment);
  void* aligned_p = (void*)((uintptr_t)p + adjust);
  if (aligned_p != p) {
    pa_page_set_has_aligned(page, true);
    #if PA_GUARDED
    // set tag to aligned so pa_usable_size works with guard pages
    if (adjust >= sizeof(pa_block_t)) {
      pa_block_t* const block = (pa_block_t*)p;
      block->next = PA_BLOCK_TAG_ALIGNED;
    }
    #endif
    _pa_padding_shrink(page, (pa_block_t*)p, adjust + size);
  }
  // todo: expand padding if overallocated ?

  pa_assert_internal(pa_page_usable_block_size(page) >= adjust + size);
  pa_assert_internal(((uintptr_t)aligned_p + offset) % alignment == 0);
  pa_assert_internal(pa_usable_size(aligned_p)>=size);
  pa_assert_internal(pa_usable_size(p) == pa_usable_size(aligned_p)+adjust);
  #if PA_DEBUG > 1
  pa_page_t* const apage = _pa_ptr_page(aligned_p);
  void* unalign_p = _pa_page_ptr_unalign(apage, aligned_p);
  pa_assert_internal(p == unalign_p);
  #endif

  // now zero the block if needed
  if (alignment > PA_BLOCK_ALIGNMENT_MAX) {
    // for the tracker, on huge aligned allocations only the memory from the start of the large block is defined
    pa_track_mem_undefined(aligned_p, size);
    if (zero) {
      _pa_memzero_aligned(aligned_p, pa_usable_size(aligned_p));
    }
  }

  if (p != aligned_p) {
    pa_track_align(p,aligned_p,adjust,pa_usable_size(aligned_p));
    #if PA_GUARDED
    pa_track_mem_defined(p, sizeof(pa_block_t));
    #endif
  }
  return aligned_p;
}

// Generic primitive aligned allocation -- split out for better codegen
static pa_decl_noinline void* pa_heap_malloc_zero_aligned_at_generic(pa_heap_t* const heap, const size_t size, const size_t alignment, const size_t offset, const bool zero, size_t* usable) pa_attr_noexcept
{
  pa_assert_internal(alignment != 0 && _pa_is_power_of_two(alignment));
  // we don't allocate more than PA_MAX_ALLOC_SIZE (see <https://sourceware.org/ml/libc-announce/2019/msg00001.html>)
  if pa_unlikely(size > (PA_MAX_ALLOC_SIZE - PA_PADDING_SIZE)) {
    #if PA_DEBUG > 0
    _pa_error_message(EOVERFLOW, "aligned allocation request is too large (size %zu, alignment %zu)\n", size, alignment);
    #endif
    return NULL;
  }

  // use regular allocation if it is guaranteed to fit the alignment constraints.
  // this is important to try as the fast path in `pa_heap_malloc_zero_aligned` only works when there exist
  // a page with the right block size, and if we always use the over-alloc fallback that would never happen.
  if (offset == 0 && pa_malloc_is_naturally_aligned(size,alignment)) {
    void* p = pa_heap_malloc_zero_no_guarded(heap, size, zero, usable);
    pa_assert_internal(p == NULL || ((uintptr_t)p % alignment) == 0);
    const bool is_aligned_or_null = (((uintptr_t)p) & (alignment-1))==0;
    if pa_likely(is_aligned_or_null) {
      return p;
    }
    else {
      // this should never happen if the `pa_malloc_is_naturally_aligned` check is correct..
      pa_assert(false);
      pa_free(p);
    }
  }

  // fall back to over-allocation
  return pa_heap_malloc_zero_aligned_at_overalloc(heap,size,alignment,offset,zero,usable);
}


// Primitive aligned allocation
static void* pa_heap_malloc_zero_aligned_at(pa_heap_t* const heap, const size_t size, 
                                            const size_t alignment, const size_t offset, const bool zero,
                                            size_t* usable) pa_attr_noexcept
{
  // note: we don't require `size > offset`, we just guarantee that the address at offset is aligned regardless of the allocated size.
  if pa_unlikely(alignment == 0 || !_pa_is_power_of_two(alignment)) { // require power-of-two (see <https://en.cppreference.com/w/c/memory/aligned_alloc>)
    #if PA_DEBUG > 0
    _pa_error_message(EOVERFLOW, "aligned allocation requires the alignment to be a power-of-two (size %zu, alignment %zu)\n", size, alignment);
    #endif
    return NULL;
  }

  #if PA_GUARDED
  if (offset==0 && alignment < PA_BLOCK_ALIGNMENT_MAX && pa_heap_malloc_use_guarded(heap,size)) {
    return pa_heap_malloc_guarded_aligned(heap, size, alignment, zero);
  }
  #endif

  // try first if there happens to be a small block available with just the right alignment
  if pa_likely(size <= PA_SMALL_SIZE_MAX && alignment <= size) {
    const uintptr_t align_mask = alignment-1;       // for any x, `(x & align_mask) == (x % alignment)`
    const size_t padsize = size + PA_PADDING_SIZE;
    pa_page_t* page = _pa_heap_get_free_small_page(heap, padsize);
    if pa_likely(page->free != NULL) {
      const bool is_aligned = (((uintptr_t)page->free + offset) & align_mask)==0;
      if pa_likely(is_aligned)
      {
        if (usable!=NULL) { *usable = pa_page_usable_block_size(page); }
        void* p = (zero ? _pa_page_malloc_zeroed(heap,page,padsize) : _pa_page_malloc(heap,page,padsize)); // call specific page malloc for better codegen
        pa_assert_internal(p != NULL);
        pa_assert_internal(((uintptr_t)p + offset) % alignment == 0);
        pa_track_malloc(p,size,zero);
        return p;
      }
    }
  }

  // fallback to generic aligned allocation
  return pa_heap_malloc_zero_aligned_at_generic(heap, size, alignment, offset, zero, usable);
}


// ------------------------------------------------------
// Optimized pa_heap_malloc_aligned / pa_malloc_aligned
// ------------------------------------------------------

pa_decl_nodiscard pa_decl_restrict void* pa_heap_malloc_aligned_at(pa_heap_t* heap, size_t size, size_t alignment, size_t offset) pa_attr_noexcept {
  return pa_heap_malloc_zero_aligned_at(heap, size, alignment, offset, false, NULL);
}

pa_decl_nodiscard pa_decl_restrict void* pa_heap_malloc_aligned(pa_heap_t* heap, size_t size, size_t alignment) pa_attr_noexcept {
  return pa_heap_malloc_aligned_at(heap, size, alignment, 0);
}

// ensure a definition is emitted
#if defined(__cplusplus)
void* _pa_extern_heap_malloc_aligned = (void*)&pa_heap_malloc_aligned;
#endif

// ------------------------------------------------------
// Aligned Allocation
// ------------------------------------------------------

pa_decl_nodiscard pa_decl_restrict void* pa_heap_zalloc_aligned_at(pa_heap_t* heap, size_t size, size_t alignment, size_t offset) pa_attr_noexcept {
  return pa_heap_malloc_zero_aligned_at(heap, size, alignment, offset, true, NULL);
}

pa_decl_nodiscard pa_decl_restrict void* pa_heap_zalloc_aligned(pa_heap_t* heap, size_t size, size_t alignment) pa_attr_noexcept {
  return pa_heap_zalloc_aligned_at(heap, size, alignment, 0);
}

pa_decl_nodiscard pa_decl_restrict void* pa_heap_calloc_aligned_at(pa_heap_t* heap, size_t count, size_t size, size_t alignment, size_t offset) pa_attr_noexcept {
  size_t total;
  if (pa_count_size_overflow(count, size, &total)) return NULL;
  return pa_heap_zalloc_aligned_at(heap, total, alignment, offset);
}

pa_decl_nodiscard pa_decl_restrict void* pa_heap_calloc_aligned(pa_heap_t* heap, size_t count, size_t size, size_t alignment) pa_attr_noexcept {
  return pa_heap_calloc_aligned_at(heap,count,size,alignment,0);
}

pa_decl_nodiscard pa_decl_restrict void* pa_malloc_aligned_at(size_t size, size_t alignment, size_t offset) pa_attr_noexcept {
  return pa_heap_malloc_aligned_at(pa_prim_get_default_heap(), size, alignment, offset);
}

pa_decl_nodiscard pa_decl_restrict void* pa_malloc_aligned(size_t size, size_t alignment) pa_attr_noexcept {
  return pa_heap_malloc_aligned(pa_prim_get_default_heap(), size, alignment);
}

pa_decl_nodiscard pa_decl_restrict void* pa_umalloc_aligned(size_t size, size_t alignment, size_t* block_size) pa_attr_noexcept {
  return pa_heap_malloc_zero_aligned_at(pa_prim_get_default_heap(), size, alignment, 0, false, block_size);
}

pa_decl_nodiscard pa_decl_restrict void* pa_zalloc_aligned_at(size_t size, size_t alignment, size_t offset) pa_attr_noexcept {
  return pa_heap_zalloc_aligned_at(pa_prim_get_default_heap(), size, alignment, offset);
}

pa_decl_nodiscard pa_decl_restrict void* pa_zalloc_aligned(size_t size, size_t alignment) pa_attr_noexcept {
  return pa_heap_zalloc_aligned(pa_prim_get_default_heap(), size, alignment);
}

pa_decl_nodiscard pa_decl_restrict void* pa_uzalloc_aligned(size_t size, size_t alignment, size_t* block_size) pa_attr_noexcept {
  return pa_heap_malloc_zero_aligned_at(pa_prim_get_default_heap(), size, alignment, 0, true, block_size);
}

pa_decl_nodiscard pa_decl_restrict void* pa_calloc_aligned_at(size_t count, size_t size, size_t alignment, size_t offset) pa_attr_noexcept {
  return pa_heap_calloc_aligned_at(pa_prim_get_default_heap(), count, size, alignment, offset);
}

pa_decl_nodiscard pa_decl_restrict void* pa_calloc_aligned(size_t count, size_t size, size_t alignment) pa_attr_noexcept {
  return pa_heap_calloc_aligned(pa_prim_get_default_heap(), count, size, alignment);
}


// ------------------------------------------------------
// Aligned re-allocation
// ------------------------------------------------------

static void* pa_heap_realloc_zero_aligned_at(pa_heap_t* heap, void* p, size_t newsize, size_t alignment, size_t offset, bool zero) pa_attr_noexcept {
  pa_assert(alignment > 0);
  if (alignment <= sizeof(uintptr_t)) return _pa_heap_realloc_zero(heap,p,newsize,zero,NULL,NULL);
  if (p == NULL) return pa_heap_malloc_zero_aligned_at(heap,newsize,alignment,offset,zero,NULL);
  size_t size = pa_usable_size(p);
  if (newsize <= size && newsize >= (size - (size / 2))
      && (((uintptr_t)p + offset) % alignment) == 0) {
    return p;  // reallocation still fits, is aligned and not more than 50% waste
  }
  else {
    // note: we don't zero allocate upfront so we only zero initialize the expanded part
    void* newp = pa_heap_malloc_aligned_at(heap,newsize,alignment,offset);
    if (newp != NULL) {
      if (zero && newsize > size) {
        // also set last word in the previous allocation to zero to ensure any padding is zero-initialized
        size_t start = (size >= sizeof(intptr_t) ? size - sizeof(intptr_t) : 0);
        _pa_memzero((uint8_t*)newp + start, newsize - start);
      }
      _pa_memcpy_aligned(newp, p, (newsize > size ? size : newsize));
      pa_free(p); // only free if successful
    }
    return newp;
  }
}

static void* pa_heap_realloc_zero_aligned(pa_heap_t* heap, void* p, size_t newsize, size_t alignment, bool zero) pa_attr_noexcept {
  pa_assert(alignment > 0);
  if (alignment <= sizeof(uintptr_t)) return _pa_heap_realloc_zero(heap,p,newsize,zero,NULL,NULL);
  size_t offset = ((uintptr_t)p % alignment); // use offset of previous allocation (p can be NULL)
  return pa_heap_realloc_zero_aligned_at(heap,p,newsize,alignment,offset,zero);
}

pa_decl_nodiscard void* pa_heap_realloc_aligned_at(pa_heap_t* heap, void* p, size_t newsize, size_t alignment, size_t offset) pa_attr_noexcept {
  return pa_heap_realloc_zero_aligned_at(heap,p,newsize,alignment,offset,false);
}

pa_decl_nodiscard void* pa_heap_realloc_aligned(pa_heap_t* heap, void* p, size_t newsize, size_t alignment) pa_attr_noexcept {
  return pa_heap_realloc_zero_aligned(heap,p,newsize,alignment,false);
}

pa_decl_nodiscard void* pa_heap_rezalloc_aligned_at(pa_heap_t* heap, void* p, size_t newsize, size_t alignment, size_t offset) pa_attr_noexcept {
  return pa_heap_realloc_zero_aligned_at(heap, p, newsize, alignment, offset, true);
}

pa_decl_nodiscard void* pa_heap_rezalloc_aligned(pa_heap_t* heap, void* p, size_t newsize, size_t alignment) pa_attr_noexcept {
  return pa_heap_realloc_zero_aligned(heap, p, newsize, alignment, true);
}

pa_decl_nodiscard void* pa_heap_recalloc_aligned_at(pa_heap_t* heap, void* p, size_t newcount, size_t size, size_t alignment, size_t offset) pa_attr_noexcept {
  size_t total;
  if (pa_count_size_overflow(newcount, size, &total)) return NULL;
  return pa_heap_rezalloc_aligned_at(heap, p, total, alignment, offset);
}

pa_decl_nodiscard void* pa_heap_recalloc_aligned(pa_heap_t* heap, void* p, size_t newcount, size_t size, size_t alignment) pa_attr_noexcept {
  size_t total;
  if (pa_count_size_overflow(newcount, size, &total)) return NULL;
  return pa_heap_rezalloc_aligned(heap, p, total, alignment);
}

pa_decl_nodiscard void* pa_realloc_aligned_at(void* p, size_t newsize, size_t alignment, size_t offset) pa_attr_noexcept {
  return pa_heap_realloc_aligned_at(pa_prim_get_default_heap(), p, newsize, alignment, offset);
}

pa_decl_nodiscard void* pa_realloc_aligned(void* p, size_t newsize, size_t alignment) pa_attr_noexcept {
  return pa_heap_realloc_aligned(pa_prim_get_default_heap(), p, newsize, alignment);
}

pa_decl_nodiscard void* pa_rezalloc_aligned_at(void* p, size_t newsize, size_t alignment, size_t offset) pa_attr_noexcept {
  return pa_heap_rezalloc_aligned_at(pa_prim_get_default_heap(), p, newsize, alignment, offset);
}

pa_decl_nodiscard void* pa_rezalloc_aligned(void* p, size_t newsize, size_t alignment) pa_attr_noexcept {
  return pa_heap_rezalloc_aligned(pa_prim_get_default_heap(), p, newsize, alignment);
}

pa_decl_nodiscard void* pa_recalloc_aligned_at(void* p, size_t newcount, size_t size, size_t alignment, size_t offset) pa_attr_noexcept {
  return pa_heap_recalloc_aligned_at(pa_prim_get_default_heap(), p, newcount, size, alignment, offset);
}

pa_decl_nodiscard void* pa_recalloc_aligned(void* p, size_t newcount, size_t size, size_t alignment) pa_attr_noexcept {
  return pa_heap_recalloc_aligned(pa_prim_get_default_heap(), p, newcount, size, alignment);
}


