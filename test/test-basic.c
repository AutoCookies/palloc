/*
 * Version-agnostic basic API test.
 * Checks core allocator behavior and that version/API exist without
 * asserting a specific version number, so it stays valid across releases.
 */
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "palloc.h"

int main(void) {
    /* Version: must be a positive integer; we do not assert a fixed value */
    {
        int v = pa_version();
        assert(v > 0 && "pa_version() should return a positive version number");
        (void)v;
    }

    /* malloc(0) / free(NULL) are allowed */
    {
        void* p = pa_malloc(0);
        assert(p != NULL);
        pa_free(p);
        pa_free(NULL);
    }

    /* Basic malloc/free and usable_size */
    {
        size_t n = 64;
        void* p = pa_malloc(n);
        assert(p != NULL);
        assert(pa_usable_size(p) >= n);
        memset(p, 0xAB, n);
        pa_free(p);
    }

    /* calloc zeroes and overflow check */
    {
        void* p = pa_calloc(8, 16);
        assert(p != NULL);
        assert(pa_usable_size(p) >= 128);
        for (size_t i = 0; i < 128; i++)
            assert(((unsigned char*)p)[i] == 0);
        pa_free(p);
    }

    /* realloc grow and shrink */
    {
        void* p = pa_malloc(32);
        assert(p != NULL);
        p = pa_realloc(p, 64);
        assert(p != NULL);
        assert(pa_usable_size(p) >= 64);
        p = pa_realloc(p, 16);
        assert(p != NULL);
        pa_free(p);
    }

    /* realloc(NULL, size) like malloc */
    {
        void* p = pa_realloc(NULL, 100);
        assert(p != NULL);
        pa_free(p);
    }

    /* Aligned allocation when supported */
    {
        void* p = pa_malloc_aligned(256, 64);
        assert(p != NULL);
        assert(((uintptr_t)p % 64) == 0);
        pa_free_aligned(p, 64);
    }

    return 0;
}
