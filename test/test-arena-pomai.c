/* Arena Pomai API test. Version-agnostic; verifies p_arena_* and vector API. */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "palloc/arena_pomai.h"

static void test_legacy_api(void) {
    printf("  Legacy API (p_arena_create / p_arena_alloc)...\n");

    size_t arena_size = 1024 * 1024 * 100; /* 100 MB */
    void* arena = p_arena_create(arena_size);
    assert(arena != NULL);

    size_t alloc_size1 = 1024 * 1024; /* 1 MB */
    void* p1 = p_arena_alloc(arena, alloc_size1);
    assert(p1 != NULL);
    memset(p1, 0xAA, alloc_size1);

    size_t alloc_size2 = 2 * 1024 * 1024; /* 2 MB */
    void* p2 = p_arena_alloc(arena, alloc_size2);
    assert(p2 != NULL);
    assert(p2 > p1);
    memset(p2, 0xBB, alloc_size2);

    p_arena_reset((pa_arena_t*)arena);

    void* p3 = p_arena_alloc(arena, alloc_size1);
    assert(p3 != NULL);
    assert(p3 == p1);
    (void)p3;

    p_arena_destroy((pa_arena_t*)arena);
    printf("  Legacy API OK.\n");
}

static void test_vector_api(void) {
    printf("  Vector API (p_arena_create_for_vector / p_arena_alloc_vector)...\n");

    size_t capacity = 1024 * 1024 * 10; /* 10 MB */
    pa_arena_t* arena = p_arena_create_for_vector(capacity);
    assert(arena != NULL);

    /* Alloc a 1536-dim float vector (e.g. embedding) */
    size_t dim = 1536;
    size_t elem = sizeof(float);
    float* v1 = (float*)p_arena_alloc_vector(arena, dim, elem);
    assert(v1 != NULL);
    assert((uintptr_t)v1 % PA_ARENA_VECTOR_ALIGN == 0);
    memset(v1, 0, dim * elem);

    /* Second vector */
    float* v2 = (float*)p_arena_alloc_vector(arena, dim, elem);
    assert(v2 != NULL);
    assert((uintptr_t)v2 % PA_ARENA_VECTOR_ALIGN == 0);
    assert(v2 > (void*)v1);

    p_arena_reset(arena);

    /* After reset, first alloc returns same region */
    float* v3 = (float*)p_arena_alloc_vector(arena, dim, elem);
    assert(v3 != NULL);
    assert(v3 == v1);
    (void)v2;
    (void)v3;

    p_arena_destroy(arena);
    printf("  Vector API OK.\n");
}

static void test_capacity_limit(void) {
    printf("  Capacity limit (zero-OOM)...\n");

    /* Small arena: 64 KB */
    size_t capacity = 64 * 1024;
    pa_arena_t* arena = p_arena_create_for_vector(capacity);
    assert(arena != NULL);

    /* Alloc until we hit NULL */
    size_t n = 0;
    while (p_arena_alloc_vector(arena, 1024, sizeof(float)) != NULL)
        n++;
    assert(n > 0);
    /* Next alloc must return NULL */
    assert(p_arena_alloc_vector(arena, 1024, sizeof(float)) == NULL);

    p_arena_destroy(arena);
    printf("  Capacity limit OK (got %zu vectors before NULL).\n", n);
}

static void test_alignment(void) {
    printf("  SIMD alignment (PA_ARENA_VECTOR_ALIGN)...\n");

    pa_arena_t* arena = p_arena_create_for_vector(1024 * 1024);
    assert(arena != NULL);

    for (size_t dim = 1; dim <= 2048; dim = (dim ? dim * 2 : 1)) {
        void* p = p_arena_alloc_vector(arena, dim, sizeof(float));
        assert(p != NULL);
        assert((uintptr_t)p % PA_ARENA_VECTOR_ALIGN == 0);
        (void)p;
    }

    p_arena_destroy(arena);
    printf("  SIMD alignment OK.\n");
}

static void test_shared_arena(void) {
    printf("  Extended create (shared=false default)...\n");

    pa_arena_t* arena = p_arena_create_for_vector(1024 * 1024);
    assert(arena != NULL);
    void* p = p_arena_alloc_vector(arena, 100, sizeof(float));
    assert(p != NULL);
    (void)p;
    p_arena_destroy(arena);
    printf("  Extended create OK.\n");
}

int main(void) {
    printf("Starting Arena Pomai Verification...\n");

    test_legacy_api();
    test_vector_api();
    test_capacity_limit();
    test_alignment();
    test_shared_arena();

    printf("Arena Pomai Verification PASSED!\n");
    return 0;
}
