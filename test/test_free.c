//
// pmalloc_free and pmalloc_merge
//
// - freeing NULL changes nothing
// - a freed block returns its whole span to freemem and merges with a free neighbour before it,
//   after it, or both, so no two free nodes are ever adjacent
// - freeing every block, in any order, leaves one free node covering the region
//

#include "test_support.h"

static pmalloc_t pm;
static void* mem[6];

static void setup_six_blocks(void) {
    pmalloc_init(&pm);
    pmalloc_addblock(&pm, pm_test_arena, 8192);
    for (int i = 0; i < 6; i++) mem[i] = pmalloc_malloc(&pm, 128);
}

static void test_free_null_changes_nothing(void) {
    setup_six_blocks();
    uint32_t free_bytes = pmalloc_freemem(&pm);
    pmalloc_free(&pm, NULL);
    TEST_ASSERT_EQUAL_UINT32(free_bytes, pmalloc_freemem(&pm));
    TEST_ASSERT_EQUAL_UINT32(7u, pm.totalnodes);
}

static void test_free_returns_the_whole_span(void) {
    pmalloc_init(&pm);
    pmalloc_addblock(&pm, pm_test_arena, 8192);
    void* a = pmalloc_malloc(&pm, 1);
    void* b = pmalloc_malloc(&pm, 64);
    uint32_t free_bytes = pmalloc_freemem(&pm);
    pmalloc_free(&pm, a);
    TEST_ASSERT_EQUAL_UINT32(free_bytes + PM_TEST_ALIGN, pmalloc_freemem(&pm));
    TEST_ASSERT_NOT_NULL(b);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 8192);
}

static void test_free_without_free_neighbours(void) {
    setup_six_blocks();
    pmalloc_free(&pm, mem[2]);
    TEST_ASSERT_EQUAL_UINT32(7u, pm.totalnodes);
    TEST_ASSERT_EQUAL_PTR((unsigned char*)mem[2] - PM_TEST_HDR, pm.available);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 8192);
}

static void test_free_merges_with_the_next_node(void) {
    setup_six_blocks();
    pmalloc_free(&pm, mem[3]);
    pmalloc_free(&pm, mem[2]);
    TEST_ASSERT_EQUAL_UINT32(6u, pm.totalnodes);
    TEST_ASSERT_EQUAL_UINT32(2u * 128u + PM_TEST_HDR, pm.available->size);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 8192);
}

static void test_free_merges_with_the_previous_node(void) {
    setup_six_blocks();
    pmalloc_free(&pm, mem[2]);
    pmalloc_free(&pm, mem[3]);
    TEST_ASSERT_EQUAL_UINT32(6u, pm.totalnodes);
    TEST_ASSERT_EQUAL_UINT32(2u * 128u + PM_TEST_HDR, pm.available->size);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 8192);
}

static void test_free_merges_with_both_neighbours(void) {
    setup_six_blocks();
    pmalloc_free(&pm, mem[1]);
    pmalloc_free(&pm, mem[3]);
    pmalloc_free(&pm, mem[2]);
    TEST_ASSERT_EQUAL_UINT32(5u, pm.totalnodes);
    TEST_ASSERT_EQUAL_UINT32(3u * 128u + 2u * PM_TEST_HDR, pm.available->size);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 8192);
}

static void test_free_merges_into_the_tail(void) {
    setup_six_blocks();
    pmalloc_free(&pm, mem[5]);
    TEST_ASSERT_EQUAL_UINT32(6u, pm.totalnodes);
    TEST_ASSERT_NULL(pm.available->next);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 8192);
}

static void test_free_everything_in_any_order(void) {
    static const int orders[4][6] = {
        {0, 1, 2, 3, 4, 5}, {5, 4, 3, 2, 1, 0}, {1, 3, 5, 0, 2, 4}, {2, 0, 4, 5, 1, 3}};
    for (int o = 0; o < 4; o++) {
        setup_six_blocks();
        for (int i = 0; i < 6; i++) {
            pmalloc_free(&pm, mem[orders[o][i]]);
            pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 8192);
        }
        TEST_ASSERT_EQUAL_UINT32(1u, pm.totalnodes);
        TEST_ASSERT_EQUAL_UINT32(8192u - PM_TEST_HDR, pmalloc_freemem(&pm));
        TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_usedmem(&pm));
    }
}

void run_free_tests(void) {
    UnitySetTestFile(__FILE__);
    RUN_TEST(test_free_null_changes_nothing);
    RUN_TEST(test_free_returns_the_whole_span);
    RUN_TEST(test_free_without_free_neighbours);
    RUN_TEST(test_free_merges_with_the_next_node);
    RUN_TEST(test_free_merges_with_the_previous_node);
    RUN_TEST(test_free_merges_with_both_neighbours);
    RUN_TEST(test_free_merges_into_the_tail);
    RUN_TEST(test_free_everything_in_any_order);
}
