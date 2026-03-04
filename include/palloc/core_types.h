/* ----------------------------------------------------------------------------
 * Core types for palloc dual-mode build. Pure C: stddef + stdint only.
 * Layout of palloc_vector_t is fixed for zero-copy between USER and KERNEL.
 * -------------------------------------------------------------------------- */
#pragma once
#ifndef PALLOC_CORE_TYPES_H
#define PALLOC_CORE_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Alignment for all allocations (ARM NEON 128-bit). */
#define PALLOC_ALIGNMENT  16

/**
 * Vector descriptor: fixed layout, identical in USER and KERNEL for zero-copy.
 * capacity = allocated capacity in bytes; length = used length in bytes;
 * data = pointer to contiguous storage (16-byte aligned).
 */
typedef struct palloc_vector {
  size_t capacity;
  size_t length;
  void  *data;
  /* Reserved for future use; keeps struct size stable. */
  uintptr_t _reserved;
} palloc_vector_t;

#ifdef __cplusplus
}
#endif

#endif /* PALLOC_CORE_TYPES_H */
