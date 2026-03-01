/* ----------------------------------------------------------------------------
Copyright (c) 2019-2023 Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

/* ----------------------------------------------------------------------------
Concurrent bitmap that can set/reset sequences of bits atomically,
represented as an array of fields where each field is a machine word (`size_t`)

There are two api's; the standard one cannot have sequences that cross
between the bitmap fields (and a sequence must be <= PA_BITMAP_FIELD_BITS).
(this is used in region allocation)

The `_across` postfixed functions do allow sequences that can cross over
between the fields. (This is used in arena allocation)
---------------------------------------------------------------------------- */
#pragma once
#ifndef PA_BITMAP_H
#define PA_BITMAP_H

/* -----------------------------------------------------------
  Bitmap definition
----------------------------------------------------------- */

#define PA_BITMAP_FIELD_BITS   (8*PA_SIZE_SIZE)
#define PA_BITMAP_FIELD_FULL   (~((size_t)0))   // all bits set

// An atomic bitmap of `size_t` fields
typedef _Atomic(size_t)  pa_bitmap_field_t;
typedef pa_bitmap_field_t*  pa_bitmap_t;

// A bitmap index is the index of the bit in a bitmap.
typedef size_t pa_bitmap_index_t;

// Create a bit index.
static inline pa_bitmap_index_t pa_bitmap_index_create_ex(size_t idx, size_t bitidx) {
  pa_assert_internal(bitidx <= PA_BITMAP_FIELD_BITS);
  return (idx*PA_BITMAP_FIELD_BITS) + bitidx;
}
static inline pa_bitmap_index_t pa_bitmap_index_create(size_t idx, size_t bitidx) {
  pa_assert_internal(bitidx < PA_BITMAP_FIELD_BITS);
  return pa_bitmap_index_create_ex(idx,bitidx);
}

// Create a bit index.
static inline pa_bitmap_index_t pa_bitmap_index_create_from_bit(size_t full_bitidx) {  
  return pa_bitmap_index_create(full_bitidx / PA_BITMAP_FIELD_BITS, full_bitidx % PA_BITMAP_FIELD_BITS);
}

// Get the field index from a bit index.
static inline size_t pa_bitmap_index_field(pa_bitmap_index_t bitmap_idx) {
  return (bitmap_idx / PA_BITMAP_FIELD_BITS);
}

// Get the bit index in a bitmap field
static inline size_t pa_bitmap_index_bit_in_field(pa_bitmap_index_t bitmap_idx) {
  return (bitmap_idx % PA_BITMAP_FIELD_BITS);
}

// Get the full bit index
static inline size_t pa_bitmap_index_bit(pa_bitmap_index_t bitmap_idx) {
  return bitmap_idx;
}

/* -----------------------------------------------------------
  Claim a bit sequence atomically
----------------------------------------------------------- */

// Try to atomically claim a sequence of `count` bits in a single
// field at `idx` in `bitmap`. Returns `true` on success.
bool _pa_bitmap_try_find_claim_field(pa_bitmap_t bitmap, size_t idx, const size_t count, pa_bitmap_index_t* bitmap_idx);

// Starts at idx, and wraps around to search in all `bitmap_fields` fields.
// For now, `count` can be at most PA_BITMAP_FIELD_BITS and will never cross fields.
bool _pa_bitmap_try_find_from_claim(pa_bitmap_t bitmap, const size_t bitmap_fields, const size_t start_field_idx, const size_t count, pa_bitmap_index_t* bitmap_idx);

// Like _pa_bitmap_try_find_from_claim but with an extra predicate that must be fullfilled
typedef bool (pa_cdecl *pa_bitmap_pred_fun_t)(pa_bitmap_index_t bitmap_idx, void* pred_arg);
bool _pa_bitmap_try_find_from_claim_pred(pa_bitmap_t bitmap, const size_t bitmap_fields, const size_t start_field_idx, const size_t count, pa_bitmap_pred_fun_t pred_fun, void* pred_arg, pa_bitmap_index_t* bitmap_idx);

// Set `count` bits at `bitmap_idx` to 0 atomically
// Returns `true` if all `count` bits were 1 previously.
bool _pa_bitmap_unclaim(pa_bitmap_t bitmap, size_t bitmap_fields, size_t count, pa_bitmap_index_t bitmap_idx);

// Try to set `count` bits at `bitmap_idx` from 0 to 1 atomically. 
// Returns `true` if successful when all previous `count` bits were 0.
bool _pa_bitmap_try_claim(pa_bitmap_t bitmap, size_t bitmap_fields, size_t count, pa_bitmap_index_t bitmap_idx);

// Set `count` bits at `bitmap_idx` to 1 atomically
// Returns `true` if all `count` bits were 0 previously. `any_zero` is `true` if there was at least one zero bit.
bool _pa_bitmap_claim(pa_bitmap_t bitmap, size_t bitmap_fields, size_t count, pa_bitmap_index_t bitmap_idx, bool* any_zero);

bool _pa_bitmap_is_claimed(pa_bitmap_t bitmap, size_t bitmap_fields, size_t count, pa_bitmap_index_t bitmap_idx);
bool _pa_bitmap_is_any_claimed(pa_bitmap_t bitmap, size_t bitmap_fields, size_t count, pa_bitmap_index_t bitmap_idx);


//--------------------------------------------------------------------------
// the `_across` functions work on bitmaps where sequences can cross over
// between the fields. This is used in arena allocation
//--------------------------------------------------------------------------

// Find `count` bits of zeros and set them to 1 atomically; returns `true` on success.
// Starts at idx, and wraps around to search in all `bitmap_fields` fields.
bool _pa_bitmap_try_find_from_claim_across(pa_bitmap_t bitmap, const size_t bitmap_fields, const size_t start_field_idx, const size_t count, pa_bitmap_index_t* bitmap_idx);

// Set `count` bits at `bitmap_idx` to 0 atomically
// Returns `true` if all `count` bits were 1 previously.
bool _pa_bitmap_unclaim_across(pa_bitmap_t bitmap, size_t bitmap_fields, size_t count, pa_bitmap_index_t bitmap_idx);

// Set `count` bits at `bitmap_idx` to 1 atomically
// Returns `true` if all `count` bits were 0 previously. `any_zero` is `true` if there was at least one zero bit.
bool _pa_bitmap_claim_across(pa_bitmap_t bitmap, size_t bitmap_fields, size_t count, pa_bitmap_index_t bitmap_idx, bool* pany_zero, size_t* already_set);

bool _pa_bitmap_is_claimed_across(pa_bitmap_t bitmap, size_t bitmap_fields, size_t count, pa_bitmap_index_t bitmap_idx, size_t* already_set);
bool _pa_bitmap_is_any_claimed_across(pa_bitmap_t bitmap, size_t bitmap_fields, size_t count, pa_bitmap_index_t bitmap_idx);

#endif
