/* ----------------------------------------------------------------------------
Copyright (c) 2019-2024, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

#if !defined(PA_IN_ARENA_C)
#error "this file should be included from 'arena.c' (so pa_arena_t is visible)"
// add includes help an IDE
#include "palloc.h"
#include "palloc/internal.h"
#include "bitmap.h"
#endif

// Minimal exports for arena-abandoned.
size_t      pa_arena_id_index(pa_arena_id_t id);
pa_arena_t* pa_arena_from_index(size_t idx);
size_t      pa_arena_get_count(void);
void*       pa_arena_block_start(pa_arena_t* arena, pa_bitmap_index_t bindex);
bool        pa_arena_memid_indices(pa_memid_t memid, size_t* arena_index, pa_bitmap_index_t* bitmap_index);

/* -----------------------------------------------------------
  Abandoned blocks/segments:

  _pa_arena_segment_clear_abandoned
  _pa_arena_segment_mark_abandoned

  This is used to atomically abandon/reclaim segments
  (and crosses the arena API but it is convenient to have here).

  Abandoned segments still have live blocks; they get reclaimed
  when a thread frees a block in it, or when a thread needs a fresh
  segment.

  Abandoned segments are atomically marked in the `block_abandoned`
  bitmap of arenas. Any segments allocated outside arenas are put
  in the sub-process `abandoned_os_list`. This list is accessed
  using locks but this should be uncommon and generally uncontended.
  Reclaim and visiting either scan through the `block_abandoned`
  bitmaps of the arena's, or visit the `abandoned_os_list`

  A potentially nicer design is to use arena's for everything
  and perhaps have virtual arena's to map OS allocated memory
  but this would lack the "density" of our current arena's. TBC.
----------------------------------------------------------- */


// reclaim a specific OS abandoned segment; `true` on success.
// sets the thread_id.
static bool pa_arena_segment_os_clear_abandoned(pa_segment_t* segment, bool take_lock) {
  pa_assert(segment->memid.memkind != PA_MEM_ARENA);
  // not in an arena, remove from list of abandoned os segments
  pa_subproc_t* const subproc = segment->subproc;
  if (take_lock && !pa_lock_try_acquire(&subproc->abandoned_os_lock)) {
    return false;  // failed to acquire the lock, we just give up
  }
  // remove atomically from the abandoned os list (if possible!)
  bool reclaimed = false;
  pa_segment_t* const next = segment->abandoned_os_next;
  pa_segment_t* const prev = segment->abandoned_os_prev;
  if (next != NULL || prev != NULL || subproc->abandoned_os_list == segment) {
    #if PA_DEBUG>3
    // find ourselves in the abandoned list (and check the count)
    bool found = false;
    size_t count = 0;
    for (pa_segment_t* current = subproc->abandoned_os_list; current != NULL; current = current->abandoned_os_next) {
      if (current == segment) { found = true; }
      count++;
    }
    pa_assert_internal(found);
    pa_assert_internal(count == pa_atomic_load_relaxed(&subproc->abandoned_os_list_count));
    #endif
    // remove (atomically) from the list and reclaim
    if (prev != NULL) { prev->abandoned_os_next = next; }
    else { subproc->abandoned_os_list = next; }
    if (next != NULL) { next->abandoned_os_prev = prev; }
    else { subproc->abandoned_os_list_tail = prev; }
    segment->abandoned_os_next = NULL;
    segment->abandoned_os_prev = NULL;
    pa_atomic_decrement_relaxed(&subproc->abandoned_count);
    pa_atomic_decrement_relaxed(&subproc->abandoned_os_list_count);
    if (take_lock) { // don't reset the thread_id when iterating
      pa_atomic_store_release(&segment->thread_id, _pa_thread_id());
    }
    reclaimed = true;
  }
  if (take_lock) { pa_lock_release(&segment->subproc->abandoned_os_lock); }
  return reclaimed;
}

// reclaim a specific abandoned segment; `true` on success.
// sets the thread_id.
bool _pa_arena_segment_clear_abandoned(pa_segment_t* segment) {
  if pa_unlikely(segment->memid.memkind != PA_MEM_ARENA) {
    return pa_arena_segment_os_clear_abandoned(segment, true /* take lock */);
  }
  // arena segment: use the blocks_abandoned bitmap.
  size_t arena_idx;
  size_t bitmap_idx;
  pa_arena_memid_indices(segment->memid, &arena_idx, &bitmap_idx);
  pa_arena_t* arena = pa_arena_from_index(arena_idx);
  pa_assert_internal(arena != NULL);
  // reclaim atomically
  bool was_marked = _pa_bitmap_unclaim(arena->blocks_abandoned, arena->field_count, 1, bitmap_idx);
  if (was_marked) {
    pa_assert_internal(pa_atomic_load_acquire(&segment->thread_id) == 0);
    pa_atomic_decrement_relaxed(&segment->subproc->abandoned_count);
    pa_atomic_store_release(&segment->thread_id, _pa_thread_id());
  }
  // pa_assert_internal(was_marked);
  pa_assert_internal(!was_marked || _pa_bitmap_is_claimed(arena->blocks_inuse, arena->field_count, 1, bitmap_idx));
  //pa_assert_internal(arena->blocks_committed == NULL || _pa_bitmap_is_claimed(arena->blocks_committed, arena->field_count, 1, bitmap_idx));
  return was_marked;
}


// mark a specific OS segment as abandoned
static void pa_arena_segment_os_mark_abandoned(pa_segment_t* segment) {
  pa_assert(segment->memid.memkind != PA_MEM_ARENA);
  // not in an arena; we use a list of abandoned segments
  pa_subproc_t* const subproc = segment->subproc;
  pa_lock(&subproc->abandoned_os_lock) {
    // push on the tail of the list (important for the visitor)
    pa_segment_t* prev = subproc->abandoned_os_list_tail;
    pa_assert_internal(prev == NULL || prev->abandoned_os_next == NULL);
    pa_assert_internal(segment->abandoned_os_prev == NULL);
    pa_assert_internal(segment->abandoned_os_next == NULL);
    if (prev != NULL) { prev->abandoned_os_next = segment; }
    else { subproc->abandoned_os_list = segment; }
    subproc->abandoned_os_list_tail = segment;
    segment->abandoned_os_prev = prev;
    segment->abandoned_os_next = NULL;
    pa_atomic_increment_relaxed(&subproc->abandoned_os_list_count);
    pa_atomic_increment_relaxed(&subproc->abandoned_count);
    // and release the lock
  }
  return;
}

// mark a specific segment as abandoned
// clears the thread_id.
void _pa_arena_segment_mark_abandoned(pa_segment_t* segment)
{
  pa_assert_internal(segment->used == segment->abandoned);
  pa_atomic_store_release(&segment->thread_id, (uintptr_t)0);  // mark as abandoned for multi-thread free's
  if pa_unlikely(segment->memid.memkind != PA_MEM_ARENA) {
    pa_arena_segment_os_mark_abandoned(segment);
    return;
  }
  // segment is in an arena, mark it in the arena `blocks_abandoned` bitmap
  size_t arena_idx;
  size_t bitmap_idx;
  pa_arena_memid_indices(segment->memid, &arena_idx, &bitmap_idx);
  pa_arena_t* arena = pa_arena_from_index(arena_idx);
  pa_assert_internal(arena != NULL);
  // set abandonment atomically
  pa_subproc_t* const subproc = segment->subproc; // don't access the segment after setting it abandoned
  const bool was_unmarked = _pa_bitmap_claim(arena->blocks_abandoned, arena->field_count, 1, bitmap_idx, NULL);
  if (was_unmarked) { pa_atomic_increment_relaxed(&subproc->abandoned_count); }
  pa_assert_internal(was_unmarked);
  pa_assert_internal(_pa_bitmap_is_claimed(arena->blocks_inuse, arena->field_count, 1, bitmap_idx));
}


/* -----------------------------------------------------------
  Iterate through the abandoned blocks/segments using a cursor.
  This is used for reclaiming and abandoned block visiting.
----------------------------------------------------------- */

// start a cursor at a randomized arena
void _pa_arena_field_cursor_init(pa_heap_t* heap, pa_subproc_t* subproc, bool visit_all, pa_arena_field_cursor_t* current) {
  pa_assert_internal(heap == NULL || heap->tld->segments.subproc == subproc);
  current->bitmap_idx = 0;
  current->subproc = subproc;
  current->visit_all = visit_all;
  current->hold_visit_lock = false;
  const size_t abandoned_count = pa_atomic_load_relaxed(&subproc->abandoned_count);
  const size_t abandoned_list_count = pa_atomic_load_relaxed(&subproc->abandoned_os_list_count);
  const size_t max_arena = pa_arena_get_count();
  if (heap != NULL && heap->arena_id != _pa_arena_id_none()) {
    // for a heap that is bound to one arena, only visit that arena
    current->start = pa_arena_id_index(heap->arena_id);
    current->end = current->start + 1;
    current->os_list_count = 0;
  }
  else {
    // otherwise visit all starting at a random location
    if (abandoned_count > abandoned_list_count && max_arena > 0) {
      current->start = (heap == NULL || max_arena == 0 ? 0 : (pa_arena_id_t)(_pa_heap_random_next(heap) % max_arena));
      current->end = current->start + max_arena;
    }
    else {
      current->start = 0;
      current->end = 0;
    }
    current->os_list_count = abandoned_list_count; // max entries to visit in the os abandoned list
  }
  pa_assert_internal(current->start <= max_arena);
}

void _pa_arena_field_cursor_done(pa_arena_field_cursor_t* current) {
  if (current->hold_visit_lock) {
    pa_lock_release(&current->subproc->abandoned_os_visit_lock);
    current->hold_visit_lock = false;
  }
}

static pa_segment_t* pa_arena_segment_clear_abandoned_at(pa_arena_t* arena, pa_subproc_t* subproc, pa_bitmap_index_t bitmap_idx) {
  // try to reclaim an abandoned segment in the arena atomically
  if (!_pa_bitmap_unclaim(arena->blocks_abandoned, arena->field_count, 1, bitmap_idx)) return NULL;
  pa_assert_internal(_pa_bitmap_is_claimed(arena->blocks_inuse, arena->field_count, 1, bitmap_idx));
  pa_segment_t* segment = (pa_segment_t*)pa_arena_block_start(arena, bitmap_idx);
  pa_assert_internal(pa_atomic_load_relaxed(&segment->thread_id) == 0);
  // check that the segment belongs to our sub-process
  // note: this is the reason we need the `abandoned_visit` lock in the case abandoned visiting is enabled.
  //  without the lock an abandoned visit may otherwise fail to visit all abandoned segments in the sub-process.
  //  for regular reclaim it is fine to miss one sometimes so without abandoned visiting we don't need the `abandoned_visit` lock.
  if (segment->subproc != subproc) {
    // it is from another sub-process, re-mark it and continue searching
    const bool was_zero = _pa_bitmap_claim(arena->blocks_abandoned, arena->field_count, 1, bitmap_idx, NULL);
    pa_assert_internal(was_zero); PA_UNUSED(was_zero);
    return NULL;
  }
  else {
    // success, we unabandoned a segment in our sub-process
    pa_atomic_decrement_relaxed(&subproc->abandoned_count);
    return segment;
  }
}

static pa_segment_t* pa_arena_segment_clear_abandoned_next_field(pa_arena_field_cursor_t* previous) {
  const size_t max_arena = pa_arena_get_count();
  size_t field_idx = pa_bitmap_index_field(previous->bitmap_idx);
  size_t bit_idx = pa_bitmap_index_bit_in_field(previous->bitmap_idx);
  // visit arena's (from the previous cursor)
  for (; previous->start < previous->end; previous->start++, field_idx = 0, bit_idx = 0) {
    // index wraps around
    size_t arena_idx = (previous->start >= max_arena ? previous->start % max_arena : previous->start);
    pa_arena_t* arena = pa_arena_from_index(arena_idx);
    if (arena != NULL) {
      bool has_lock = false;
      // visit the abandoned fields (starting at previous_idx)
      for (; field_idx < arena->field_count; field_idx++, bit_idx = 0) {
        size_t field = pa_atomic_load_relaxed(&arena->blocks_abandoned[field_idx]);
        if pa_unlikely(field != 0) { // skip zero fields quickly
          // we only take the arena lock if there are actually abandoned segments present
          if (!has_lock && pa_option_is_enabled(pa_option_visit_abandoned)) {
            has_lock = (previous->visit_all ? (pa_lock_acquire(&arena->abandoned_visit_lock),true) : pa_lock_try_acquire(&arena->abandoned_visit_lock));
            if (!has_lock) {
              if (previous->visit_all) {
                _pa_error_message(EFAULT, "internal error: failed to visit all abandoned segments due to failure to acquire the visitor lock");
              }
              // skip to next arena
              break;
            }
          }
          pa_assert_internal(has_lock || !pa_option_is_enabled(pa_option_visit_abandoned));
          // visit each set bit in the field  (todo: maybe use `ctz` here?)
          for (; bit_idx < PA_BITMAP_FIELD_BITS; bit_idx++) {
            // pre-check if the bit is set
            size_t mask = ((size_t)1 << bit_idx);
            if pa_unlikely((field & mask) == mask) {
              pa_bitmap_index_t bitmap_idx = pa_bitmap_index_create(field_idx, bit_idx);
              pa_segment_t* const segment = pa_arena_segment_clear_abandoned_at(arena, previous->subproc, bitmap_idx);
              if (segment != NULL) {
                //pa_assert_internal(arena->blocks_committed == NULL || _pa_bitmap_is_claimed(arena->blocks_committed, arena->field_count, 1, bitmap_idx));
                if (has_lock) { pa_lock_release(&arena->abandoned_visit_lock); }
                previous->bitmap_idx = pa_bitmap_index_create_ex(field_idx, bit_idx + 1); // start at next one for the next iteration
                return segment;
              }
            }
          }
        }
      }
      if (has_lock) { pa_lock_release(&arena->abandoned_visit_lock); }
    }
  }
  return NULL;
}

static pa_segment_t* pa_arena_segment_clear_abandoned_next_list(pa_arena_field_cursor_t* previous) {
  // go through the abandoned_os_list
  // we only allow one thread per sub-process to do to visit guarded by the `abandoned_os_visit_lock`.
  // The lock is released when the cursor is released.
  if (!previous->hold_visit_lock) {
    previous->hold_visit_lock = (previous->visit_all ? (pa_lock_acquire(&previous->subproc->abandoned_os_visit_lock),true)
                                                     : pa_lock_try_acquire(&previous->subproc->abandoned_os_visit_lock));
    if (!previous->hold_visit_lock) {
      if (previous->visit_all) {
        _pa_error_message(EFAULT, "internal error: failed to visit all abandoned segments due to failure to acquire the OS visitor lock");
      }
      return NULL; // we cannot get the lock, give up
    }
  }
  // One list entry at a time
  while (previous->os_list_count > 0) {
    previous->os_list_count--;
    pa_lock_acquire(&previous->subproc->abandoned_os_lock); // this could contend with concurrent OS block abandonment and reclaim from `free`
    pa_segment_t* segment = previous->subproc->abandoned_os_list;
    // pop from head of the list, a subsequent mark will push at the end (and thus we iterate through os_list_count entries)
    if (segment == NULL || pa_arena_segment_os_clear_abandoned(segment, false /* we already have the lock */)) {
      pa_lock_release(&previous->subproc->abandoned_os_lock);
      return segment;
    }
    // already abandoned, try again
    pa_lock_release(&previous->subproc->abandoned_os_lock);
  }
  // done
  pa_assert_internal(previous->os_list_count == 0);
  return NULL;
}


// reclaim abandoned segments
// this does not set the thread id (so it appears as still abandoned)
pa_segment_t* _pa_arena_segment_clear_abandoned_next(pa_arena_field_cursor_t* previous) {
  if (previous->start < previous->end) {
    // walk the arena
    pa_segment_t* segment = pa_arena_segment_clear_abandoned_next_field(previous);
    if (segment != NULL) { return segment; }
  }
  // no entries in the arena's anymore, walk the abandoned OS list
  pa_assert_internal(previous->start == previous->end);
  return pa_arena_segment_clear_abandoned_next_list(previous);
}


bool pa_abandoned_visit_blocks(pa_subproc_id_t subproc_id, int heap_tag, bool visit_blocks, pa_block_visit_fun* visitor, void* arg) {
  // (unfortunately) the visit_abandoned option must be enabled from the start.
  // This is to avoid taking locks if abandoned list visiting is not required (as for most programs)
  if (!pa_option_is_enabled(pa_option_visit_abandoned)) {
    _pa_error_message(EFAULT, "internal error: can only visit abandoned blocks when PALLOC_VISIT_ABANDONED=ON");
    return false;
  }
  pa_arena_field_cursor_t current;
  _pa_arena_field_cursor_init(NULL, _pa_subproc_from_id(subproc_id), true /* visit all (blocking) */, &current);
  pa_segment_t* segment;
  bool ok = true;
  while (ok && (segment = _pa_arena_segment_clear_abandoned_next(&current)) != NULL) {
    ok = _pa_segment_visit_blocks(segment, heap_tag, visit_blocks, visitor, arg);
    _pa_arena_segment_mark_abandoned(segment);
  }
  _pa_arena_field_cursor_done(&current);
  return ok;
}
