/* Arena Pomai API test. Version-agnostic; verifies p_arena_* behavior. */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "palloc/arena_pomai.h"

int main(void) {
    printf("Starting Arena Pomai Verification...\n");

    size_t arena_size = 1024 * 1024 * 100; /* 100 MB */
    void* arena = p_arena_create(arena_size);
    assert(arena != NULL);
    printf("Arena created successfully.\n");

    /* Test first allocation */
    size_t alloc_size1 = 1024 * 1024; /* 1 MB */
    void* p1 = p_arena_alloc(arena, alloc_size1);
    assert(p1 != NULL);
    memset(p1, 0xAA, alloc_size1);
    printf("Allocation 1 (1MB) success.\n");

    /* Test second allocation (bump) */
    size_t alloc_size2 = 2 * 1024 * 1024; /* 2 MB */
    void* p2 = p_arena_alloc(arena, alloc_size2);
    assert(p2 != NULL);
    assert(p2 > p1);
    memset(p2, 0xBB, alloc_size2);
    printf("Allocation 2 (2MB) success (bump-pointer confirmed).\n");

    /* Test reset */
    p_arena_reset(arena);
    printf("Arena reset successful.\n");

    /* Test allocation after reset (same address as p1) */
    void* p3 = p_arena_alloc(arena, alloc_size1);
    assert(p3 != NULL);
    assert(p3 == p1);
    (void)p3;
    printf("Allocation after reset success (O(1) reset confirmed).\n");

    /* Test destruction */
    p_arena_destroy(arena);
    printf("Arena destroyed successfully.\n");

    printf("Arena Pomai Verification PASSED!\n");
    return 0;
}
