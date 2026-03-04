/* ----------------------------------------------------------------------------
 * palloc backend API: interface that the core calls; backends implement.
 * Freestanding-safe: only stddef.h and stdint.h. No stdio/stdlib.
 * -------------------------------------------------------------------------- */
#pragma once
#ifndef PALLOC_BACKEND_API_H
#define PALLOC_BACKEND_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Default page size in bytes (backend may use a getter instead). */
#ifndef PALLOC_PAGE_SIZE
#define PALLOC_PAGE_SIZE  4096
#endif

/**
 * Request a contiguous region of (num_pages * page_size) bytes from the backend.
 * Returns pointer to the region, or NULL on failure.
 * USER: virtual memory (e.g. mmap); KERNEL: physical / identity-mapped RAM.
 */
void *palloc_backend_request_pages(size_t num_pages);

/**
 * Return a region previously obtained from palloc_backend_request_pages.
 * ptr must be the exact pointer returned; num_pages must match the request.
 */
void palloc_backend_release_pages(void *ptr, size_t num_pages);

/**
 * Fatal unrecoverable error; does not return.
 * USER: e.g. abort() or exit(1); KERNEL: kernel panic or halt.
 */
void palloc_backend_panic(const char *msg);

/**
 * Optional: one-time init before any request/release.
 * USER: e.g. cache page size; KERNEL: validate memory map.
 */
void palloc_backend_init(void);

/**
 * Optional: page size in bytes (if not fixed at compile time).
 */
size_t palloc_backend_page_size(void);

#ifdef __cplusplus
}
#endif

#endif /* PALLOC_BACKEND_API_H */
