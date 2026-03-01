/*----------------------------------------------------------------------------
Copyright (c) 2018-2024, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

/* -----------------------------------------------------------
  Definition of page queues for each block size
----------------------------------------------------------- */

#ifndef PA_IN_PAGE_C
#error "this file should be included from 'page.c'"
// include to help an IDE
#include "palloc.h"
#include "palloc/internal.h"
#include "palloc/atomic.h"
#endif

/* -----------------------------------------------------------
  Minimal alignment in machine words (i.e. `sizeof(void*)`)
----------------------------------------------------------- */

#if (PA_MAX_ALIGN_SIZE > 4*PA_INTPTR_SIZE)
  #error "define alignment for more than 4x word size for this platform"
#elif (PA_MAX_ALIGN_SIZE > 2*PA_INTPTR_SIZE)
  #define PA_ALIGN4W   // 4 machine words minimal alignment
#elif (PA_MAX_ALIGN_SIZE > PA_INTPTR_SIZE)
  #define PA_ALIGN2W   // 2 machine words minimal alignment
#else
  // ok, default alignment is 1 word
#endif


/* -----------------------------------------------------------
  Queue query
----------------------------------------------------------- */


static inline bool pa_page_queue_is_huge(const pa_page_queue_t* pq) {
  return (pq->block_size == (PA_MEDIUM_OBJ_SIZE_MAX+sizeof(uintptr_t)));
}

static inline bool pa_page_queue_is_full(const pa_page_queue_t* pq) {
  return (pq->block_size == (PA_MEDIUM_OBJ_SIZE_MAX+(2*sizeof(uintptr_t))));
}

static inline bool pa_page_queue_is_special(const pa_page_queue_t* pq) {
  return (pq->block_size > PA_MEDIUM_OBJ_SIZE_MAX);
}

/* -----------------------------------------------------------
  Bins
----------------------------------------------------------- */

// Return the bin for a given field size.
// Returns PA_BIN_HUGE if the size is too large.
// We use `wsize` for the size in "machine word sizes",
// i.e. byte size == `wsize*sizeof(void*)`.
static inline size_t pa_bin(size_t size) {
  size_t wsize = _pa_wsize_from_size(size);
#if defined(PA_ALIGN4W)
  if pa_likely(wsize <= 4) {
    return (wsize <= 1 ? 1 : (wsize+1)&~1); // round to double word sizes
  }
#elif defined(PA_ALIGN2W)
  if pa_likely(wsize <= 8) {
    return (wsize <= 1 ? 1 : (wsize+1)&~1); // round to double word sizes
  }
#else
  if pa_likely(wsize <= 8) {
    return (wsize == 0 ? 1 : wsize);
  }
#endif
  else if pa_unlikely(wsize > PA_MEDIUM_OBJ_WSIZE_MAX) {
    return PA_BIN_HUGE;
  }
  else {
    #if defined(PA_ALIGN4W)
    if (wsize <= 16) { wsize = (wsize+3)&~3; } // round to 4x word sizes
    #endif
    wsize--;
    // find the highest bit
    const size_t b = (PA_SIZE_BITS - 1 - pa_clz(wsize));  // note: wsize != 0
    // and use the top 3 bits to determine the bin (~12.5% worst internal fragmentation).
    // - adjust with 3 because we use do not round the first 8 sizes
    //   which each get an exact bin
    const size_t bin = ((b << 2) + ((wsize >> (b - 2)) & 0x03)) - 3;
    pa_assert_internal(bin > 0 && bin < PA_BIN_HUGE);
    return bin;
  }
}



/* -----------------------------------------------------------
  Queue of pages with free blocks
----------------------------------------------------------- */

size_t _pa_bin(size_t size) {
  return pa_bin(size);
}

size_t _pa_bin_size(size_t bin) {
  return _pa_heap_empty.pages[bin].block_size;
}

// Good size for allocation
size_t pa_good_size(size_t size) pa_attr_noexcept {
  if (size <= PA_MEDIUM_OBJ_SIZE_MAX) {
    return _pa_bin_size(pa_bin(size + PA_PADDING_SIZE));
  }
  else {
    return _pa_align_up(size + PA_PADDING_SIZE,_pa_os_page_size());
  }
}

#if (PA_DEBUG>1)
static bool pa_page_queue_contains(pa_page_queue_t* queue, const pa_page_t* page) {
  pa_assert_internal(page != NULL);
  pa_page_t* list = queue->first;
  while (list != NULL) {
    pa_assert_internal(list->next == NULL || list->next->prev == list);
    pa_assert_internal(list->prev == NULL || list->prev->next == list);
    if (list == page) break;
    list = list->next;
  }
  return (list == page);
}

#endif

#if (PA_DEBUG>1)
static bool pa_heap_contains_queue(const pa_heap_t* heap, const pa_page_queue_t* pq) {
  return (pq >= &heap->pages[0] && pq <= &heap->pages[PA_BIN_FULL]);
}
#endif

static inline bool pa_page_is_large_or_huge(const pa_page_t* page) {
  return (pa_page_block_size(page) > PA_MEDIUM_OBJ_SIZE_MAX || pa_page_is_huge(page));
}

static size_t pa_page_bin(const pa_page_t* page) {
  const size_t bin = (pa_page_is_in_full(page) ? PA_BIN_FULL : (pa_page_is_huge(page) ? PA_BIN_HUGE : pa_bin(pa_page_block_size(page))));
  pa_assert_internal(bin <= PA_BIN_FULL);
  return bin;
}

// returns the page bin without using PA_BIN_FULL for statistics
size_t _pa_page_stats_bin(const pa_page_t* page) {
  const size_t bin = (pa_page_is_huge(page) ? PA_BIN_HUGE : pa_bin(pa_page_block_size(page)));
  pa_assert_internal(bin <= PA_BIN_HUGE);
  return bin;
}

static pa_page_queue_t* pa_heap_page_queue_of(pa_heap_t* heap, const pa_page_t* page) {
  pa_assert_internal(heap!=NULL);
  const size_t bin = pa_page_bin(page);
  pa_page_queue_t* pq = &heap->pages[bin];
  pa_assert_internal((pa_page_block_size(page) == pq->block_size) ||
                       (pa_page_is_large_or_huge(page) && pa_page_queue_is_huge(pq)) ||
                         (pa_page_is_in_full(page) && pa_page_queue_is_full(pq)));
  return pq;
}

static pa_page_queue_t* pa_page_queue_of(const pa_page_t* page) {
  pa_heap_t* heap = pa_page_heap(page);
  pa_page_queue_t* pq = pa_heap_page_queue_of(heap, page);
  pa_assert_expensive(pa_page_queue_contains(pq, page));
  return pq;
}

// The current small page array is for efficiency and for each
// small size (up to 256) it points directly to the page for that
// size without having to compute the bin. This means when the
// current free page queue is updated for a small bin, we need to update a
// range of entries in `_pa_page_small_free`.
static inline void pa_heap_queue_first_update(pa_heap_t* heap, const pa_page_queue_t* pq) {
  pa_assert_internal(pa_heap_contains_queue(heap,pq));
  size_t size = pq->block_size;
  if (size > PA_SMALL_SIZE_MAX) return;

  pa_page_t* page = pq->first;
  if (pq->first == NULL) page = (pa_page_t*)&_pa_page_empty;

  // find index in the right direct page array
  size_t start;
  size_t idx = _pa_wsize_from_size(size);
  pa_page_t** pages_free = heap->pages_free_direct;

  if (pages_free[idx] == page) return;  // already set

  // find start slot
  if (idx<=1) {
    start = 0;
  }
  else {
    // find previous size; due to minimal alignment upto 3 previous bins may need to be skipped
    size_t bin = pa_bin(size);
    const pa_page_queue_t* prev = pq - 1;
    while( bin == pa_bin(prev->block_size) && prev > &heap->pages[0]) {
      prev--;
    }
    start = 1 + _pa_wsize_from_size(prev->block_size);
    if (start > idx) start = idx;
  }

  // set size range to the right page
  pa_assert(start <= idx);
  for (size_t sz = start; sz <= idx; sz++) {
    pages_free[sz] = page;
  }
}

/*
static bool pa_page_queue_is_empty(pa_page_queue_t* queue) {
  return (queue->first == NULL);
}
*/

static void pa_page_queue_remove(pa_page_queue_t* queue, pa_page_t* page) {
  pa_assert_internal(page != NULL);
  pa_assert_expensive(pa_page_queue_contains(queue, page));
  pa_assert_internal(pa_page_block_size(page) == queue->block_size ||
                      (pa_page_is_large_or_huge(page) && pa_page_queue_is_huge(queue)) ||
                        (pa_page_is_in_full(page) && pa_page_queue_is_full(queue)));
  pa_heap_t* heap = pa_page_heap(page);

  if (page->prev != NULL) page->prev->next = page->next;
  if (page->next != NULL) page->next->prev = page->prev;
  if (page == queue->last)  queue->last = page->prev;
  if (page == queue->first) {
    queue->first = page->next;
    // update first
    pa_assert_internal(pa_heap_contains_queue(heap, queue));
    pa_heap_queue_first_update(heap,queue);
  }
  heap->page_count--;
  page->next = NULL;
  page->prev = NULL;
  // pa_atomic_store_ptr_release(pa_atomic_cast(void*, &page->heap), NULL);
  pa_page_set_in_full(page,false);
}


static void pa_page_queue_push(pa_heap_t* heap, pa_page_queue_t* queue, pa_page_t* page) {
  pa_assert_internal(pa_page_heap(page) == heap);
  pa_assert_internal(!pa_page_queue_contains(queue, page));
  #if PA_HUGE_PAGE_ABANDON
  pa_assert_internal(_pa_page_segment(page)->kind != PA_SEGMENT_HUGE);
  #endif
  pa_assert_internal(pa_page_block_size(page) == queue->block_size ||
                      (pa_page_is_large_or_huge(page) && pa_page_queue_is_huge(queue)) ||
                        (pa_page_is_in_full(page) && pa_page_queue_is_full(queue)));

  pa_page_set_in_full(page, pa_page_queue_is_full(queue));
  // pa_atomic_store_ptr_release(pa_atomic_cast(void*, &page->heap), heap);
  page->next = queue->first;
  page->prev = NULL;
  if (queue->first != NULL) {
    pa_assert_internal(queue->first->prev == NULL);
    queue->first->prev = page;
    queue->first = page;
  }
  else {
    queue->first = queue->last = page;
  }

  // update direct
  pa_heap_queue_first_update(heap, queue);
  heap->page_count++;
}

static void pa_page_queue_move_to_front(pa_heap_t* heap, pa_page_queue_t* queue, pa_page_t* page) {
  pa_assert_internal(pa_page_heap(page) == heap);
  pa_assert_internal(pa_page_queue_contains(queue, page));
  if (queue->first == page) return;
  pa_page_queue_remove(queue, page);
  pa_page_queue_push(heap, queue, page);
  pa_assert_internal(queue->first == page);
}

static void pa_page_queue_enqueue_from_ex(pa_page_queue_t* to, pa_page_queue_t* from, bool enqueue_at_end, pa_page_t* page) {
  pa_assert_internal(page != NULL);
  pa_assert_expensive(pa_page_queue_contains(from, page));
  pa_assert_expensive(!pa_page_queue_contains(to, page));
  const size_t bsize = pa_page_block_size(page);
  PA_UNUSED(bsize);
  pa_assert_internal((bsize == to->block_size && bsize == from->block_size) ||
                     (bsize == to->block_size && pa_page_queue_is_full(from)) ||
                     (bsize == from->block_size && pa_page_queue_is_full(to)) ||
                     (pa_page_is_large_or_huge(page) && pa_page_queue_is_huge(to)) ||
                     (pa_page_is_large_or_huge(page) && pa_page_queue_is_full(to)));

  pa_heap_t* heap = pa_page_heap(page);

  // delete from `from`
  if (page->prev != NULL) page->prev->next = page->next;
  if (page->next != NULL) page->next->prev = page->prev;
  if (page == from->last)  from->last = page->prev;
  if (page == from->first) {
    from->first = page->next;
    // update first
    pa_assert_internal(pa_heap_contains_queue(heap, from));
    pa_heap_queue_first_update(heap, from);
  }

  // insert into `to`
  if (enqueue_at_end) {
    // enqueue at the end
    page->prev = to->last;
    page->next = NULL;
    if (to->last != NULL) {
      pa_assert_internal(heap == pa_page_heap(to->last));
      to->last->next = page;
      to->last = page;
    }
    else {
      to->first = page;
      to->last = page;
      pa_heap_queue_first_update(heap, to);
    }
  }
  else {
    if (to->first != NULL) {
      // enqueue at 2nd place
      pa_assert_internal(heap == pa_page_heap(to->first));
      pa_page_t* next = to->first->next;
      page->prev = to->first;
      page->next = next;
      to->first->next = page;
      if (next != NULL) {
        next->prev = page;
      }
      else {
        to->last = page;
      }
    }
    else {
      // enqueue at the head (singleton list)
      page->prev = NULL;
      page->next = NULL;
      to->first = page;
      to->last = page;
      pa_heap_queue_first_update(heap, to);
    }
  }

  pa_page_set_in_full(page, pa_page_queue_is_full(to));
}

static void pa_page_queue_enqueue_from(pa_page_queue_t* to, pa_page_queue_t* from, pa_page_t* page) {
  pa_page_queue_enqueue_from_ex(to, from, true /* enqueue at the end */, page);
}

static void pa_page_queue_enqueue_from_full(pa_page_queue_t* to, pa_page_queue_t* from, pa_page_t* page) {
  // note: we could insert at the front to increase reuse, but it slows down certain benchmarks (like `alloc-test`)
  pa_page_queue_enqueue_from_ex(to, from, true /* enqueue at the end of the `to` queue? */, page);
}

// Only called from `pa_heap_absorb`.
size_t _pa_page_queue_append(pa_heap_t* heap, pa_page_queue_t* pq, pa_page_queue_t* append) {
  pa_assert_internal(pa_heap_contains_queue(heap,pq));
  pa_assert_internal(pq->block_size == append->block_size);

  if (append->first==NULL) return 0;

  // set append pages to new heap and count
  size_t count = 0;
  for (pa_page_t* page = append->first; page != NULL; page = page->next) {
    // inline `pa_page_set_heap` to avoid wrong assertion during absorption;
    // in this case it is ok to be delayed freeing since both "to" and "from" heap are still alive.
    pa_atomic_store_release(&page->xheap, (uintptr_t)heap);
    // set the flag to delayed free (not overriding NEVER_DELAYED_FREE) which has as a
    // side effect that it spins until any DELAYED_FREEING is finished. This ensures
    // that after appending only the new heap will be used for delayed free operations.
    _pa_page_use_delayed_free(page, PA_USE_DELAYED_FREE, false);
    count++;
  }

  if (pq->last==NULL) {
    // take over afresh
    pa_assert_internal(pq->first==NULL);
    pq->first = append->first;
    pq->last = append->last;
    pa_heap_queue_first_update(heap, pq);
  }
  else {
    // append to end
    pa_assert_internal(pq->last!=NULL);
    pa_assert_internal(append->first!=NULL);
    pq->last->next = append->first;
    append->first->prev = pq->last;
    pq->last = append->last;
  }
  return count;
}
