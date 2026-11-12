//
// pmalloc_realloc
//
// - a NULL pointer behaves as pmalloc_malloc
// - a size within the block's current span stays in place and reports the new size
// - shrinking returns the tail to the free chain when it can hold a header and PMALLOC_ALIGN
//   bytes, merging it with a free neighbour; a smaller tail stays with the block
// - growing stays in place when the block is followed directly by a free node big enough to cover
//   the difference; a remainder too small to split stays with the block
// - otherwise the contents move to a new block and the old one is freed; a block at the end of a
//   region moves without touching memory past the region
// - when no block can be found, NULL is returned and the original block is untouched
// - a size that cannot be rounded up to PMALLOC_ALIGN within 32 bits returns NULL
//

#include "test_support.h"

static pmalloc_t pm;

static void heap(uint32_t bytes) {
    pmalloc_init(&pm);
    pmalloc_addblock(&pm, pm_test_arena, bytes);
}

static void test_realloc_null_acts_as_malloc(void) {
    heap(4096);
    void* p = pmalloc_realloc(&pm, NULL, 100);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_UINT32(100u, pmalloc_sizeof(&pm, p));
    TEST_ASSERT_EQUAL_UINT32(2u, pm.totalnodes);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_realloc_same_size_returns_the_same_block(void) {
    const uint32_t len[4] = {100, 200, 300, 150};
    void* mem[4];
    heap(65536);
    for (int i = 0; i < 4; i++) mem[i] = pmalloc_malloc(&pm, len[i]);
    TEST_ASSERT_EQUAL_PTR(mem[2], pmalloc_realloc(&pm, mem[2], 300));
    TEST_ASSERT_EQUAL_UINT32(300u, pmalloc_sizeof(&pm, mem[2]));
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 65536);
}

static void test_realloc_within_the_span_stays_in_place(void) {
    heap(4096);
    void* p = pmalloc_malloc(&pm, 1);
    void* q = pmalloc_malloc(&pm, 64);
    uint32_t free_bytes = pmalloc_freemem(&pm);
    TEST_ASSERT_EQUAL_PTR(p, pmalloc_realloc(&pm, p, PM_TEST_ALIGN));
    TEST_ASSERT_EQUAL_UINT32(PM_TEST_ALIGN, pmalloc_sizeof(&pm, p));
    TEST_ASSERT_EQUAL_PTR(p, pmalloc_realloc(&pm, p, 1));
    TEST_ASSERT_EQUAL_UINT32(1u, pmalloc_sizeof(&pm, p));
    TEST_ASSERT_EQUAL_UINT32(free_bytes, pmalloc_freemem(&pm));
    TEST_ASSERT_NOT_NULL(q);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_realloc_shrink_splits_the_tail(void) {
    const uint32_t len[3] = {100, 400, 300};
    void* mem[3];
    heap(65536);
    for (int i = 0; i < 3; i++) mem[i] = pmalloc_malloc(&pm, len[i]);
    pm_test_fill(mem[1], 400, 1);
    void* p = pmalloc_realloc(&pm, mem[1], 150);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(mem[1], p, "pmalloc_realloc should shrink a block in place");
    TEST_ASSERT_EQUAL_UINT32(150u, pmalloc_sizeof(&pm, p));
    TEST_ASSERT_TRUE(pm_test_verify(p, 150, 1));
    TEST_ASSERT_EQUAL_UINT32(5u, pm.totalnodes);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 65536);
}

static void test_realloc_shrink_by_less_than_a_node_keeps_the_span(void) {
    heap(4096);
    void* p = pmalloc_malloc(&pm, 256);
    void* q = pmalloc_malloc(&pm, 64);
    uint32_t free_bytes = pmalloc_freemem(&pm);
    TEST_ASSERT_EQUAL_PTR(p, pmalloc_realloc(&pm, p, 255));
    TEST_ASSERT_EQUAL_UINT32(255u, pmalloc_sizeof(&pm, p));
    TEST_ASSERT_EQUAL_PTR(p, pmalloc_realloc(&pm, p, 256u - PM_TEST_ALIGN));
    TEST_ASSERT_EQUAL_UINT32(256u, pmalloc_sizeof(&pm, p));
    TEST_ASSERT_EQUAL_UINT32(free_bytes, pmalloc_freemem(&pm));
    TEST_ASSERT_EQUAL_UINT32(3u, pm.totalnodes);
    TEST_ASSERT_NOT_NULL(q);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_realloc_shrink_merges_the_tail_with_a_free_neighbour(void) {
    heap(4096);
    unsigned char* a = pmalloc_malloc(&pm, 256);
    void* b = pmalloc_malloc(&pm, 64);
    void* c = pmalloc_malloc(&pm, 64);
    pmalloc_free(&pm, b);
    uint32_t free_bytes = pmalloc_freemem(&pm);
    TEST_ASSERT_EQUAL_PTR(a, pmalloc_realloc(&pm, a, 64));
    TEST_ASSERT_EQUAL_UINT32(64u, pmalloc_sizeof(&pm, a));
    TEST_ASSERT_EQUAL_UINT32(free_bytes + 192u, pmalloc_freemem(&pm));
    TEST_ASSERT_EQUAL_UINT32(4u, pm.totalnodes);
    TEST_ASSERT_EQUAL_PTR(a + 64, pm.available);
    TEST_ASSERT_EQUAL_UINT32(256u, pm.available->size);
    TEST_ASSERT_NOT_NULL(c);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_realloc_grow_into_the_tail_stays_in_place(void) {
    const uint32_t len[4] = {100, 200, 300, 150};
    void* mem[4];
    heap(65536);
    for (int i = 0; i < 4; i++) mem[i] = pmalloc_malloc(&pm, len[i]);
    pm_test_fill(mem[3], 150, 3);
    TEST_ASSERT_EQUAL_PTR(mem[3], pmalloc_realloc(&pm, mem[3], 2048));
    TEST_ASSERT_EQUAL_UINT32(2048u, pmalloc_sizeof(&pm, mem[3]));
    TEST_ASSERT_TRUE(pm_test_verify(mem[3], 150, 3));
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 65536);
}

static void test_realloc_grow_into_a_freed_neighbour_stays_in_place(void) {
    const uint32_t len[4] = {100, 200, 300, 150};
    void* mem[4];
    heap(65536);
    for (int i = 0; i < 4; i++) mem[i] = pmalloc_malloc(&pm, len[i]);
    pmalloc_free(&pm, mem[2]);
    pm_test_fill(mem[1], 200, 1);
    TEST_ASSERT_EQUAL_PTR(mem[1], pmalloc_realloc(&pm, mem[1], 450));
    TEST_ASSERT_EQUAL_UINT32(450u, pmalloc_sizeof(&pm, mem[1]));
    TEST_ASSERT_TRUE(pm_test_verify(mem[1], 200, 1));
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 65536);
}

static void test_realloc_grow_absorbs_a_neighbour_too_small_to_split(void) {
    heap(4096);
    void* a = pmalloc_malloc(&pm, 64);
    void* b = pmalloc_malloc(&pm, 64);
    void* c = pmalloc_malloc(&pm, 64);
    pm_test_fill(a, 64, 1);
    pm_test_fill(c, 64, 3);
    pmalloc_free(&pm, b);
    TEST_ASSERT_EQUAL_PTR(a, pmalloc_realloc(&pm, a, 128u + PM_TEST_HDR - PM_TEST_ALIGN));
    TEST_ASSERT_EQUAL_UINT32(128u + PM_TEST_HDR, pmalloc_sizeof(&pm, a));
    TEST_ASSERT_TRUE(pm_test_verify(a, 64, 1));
    TEST_ASSERT_TRUE(pm_test_verify(c, 64, 3));
    TEST_ASSERT_EQUAL_UINT32(64u, pmalloc_sizeof(&pm, c));
    TEST_ASSERT_EQUAL_UINT32(3u, pm.totalnodes);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_realloc_grow_moves_when_the_next_block_is_assigned(void) {
    const uint32_t len[4] = {100, 200, 300, 150};
    void* mem[4];
    heap(65536);
    for (int i = 0; i < 4; i++) mem[i] = pmalloc_malloc(&pm, len[i]);
    pm_test_fill(mem[0], 100, 7);
    void* p = pmalloc_realloc(&pm, mem[0], 250);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_TRUE(p != mem[0]);
    TEST_ASSERT_EQUAL_UINT32(250u, pmalloc_sizeof(&pm, p));
    TEST_ASSERT_TRUE(pm_test_verify(p, 100, 7));
    TEST_ASSERT_EQUAL_PTR((unsigned char*)mem[0] - PM_TEST_HDR, pm.available);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 65536);
}

static void test_realloc_grow_moves_when_the_free_neighbour_is_too_small(void) {
    heap(4096);
    void* a = pmalloc_malloc(&pm, 64);
    void* b = pmalloc_malloc(&pm, 64);
    void* c = pmalloc_malloc(&pm, 64);
    pm_test_fill(a, 64, 1);
    pmalloc_free(&pm, b);
    void* p = pmalloc_realloc(&pm, a, 128u + PM_TEST_HDR + PM_TEST_ALIGN);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_TRUE((uintptr_t)p > (uintptr_t)c);
    TEST_ASSERT_TRUE(pm_test_verify(p, 64, 1));
    TEST_ASSERT_EQUAL_PTR(pm_test_arena, pm.available);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_realloc_grow_at_the_end_of_a_region_moves_without_overrun(void) {
    heap(1024);
    void* a = pmalloc_malloc(&pm, 600);
    void* b = pmalloc_malloc(&pm, 16);
    uint32_t last = pmalloc_freemem(&pm);
    void* c = pmalloc_malloc(&pm, last);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_NOT_NULL(b);
    pm_test_fill(c, last, 5);
    pmalloc_free(&pm, a);

    void* p = pmalloc_realloc(&pm, c, last + 8u);
    TEST_ASSERT_EQUAL_PTR(a, p);
    TEST_ASSERT_TRUE(pm_test_verify(p, last, 5));
    pm_test_check_poison(pm_test_arena + 1024, 1024);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 1024);
}

static void test_realloc_fails_without_space_and_keeps_the_block(void) {
    heap(4096);
    void* m0 = pmalloc_malloc(&pm, 2048);
    void* m1 = pmalloc_malloc(&pm, 1024);
    TEST_ASSERT_NOT_NULL(m0);
    pm_test_fill(m1, 1024, 9);
    TEST_ASSERT_NULL_MESSAGE(pmalloc_realloc(&pm, m1, 2048),
                             "pmalloc_realloc should return NULL on not enough space");
    TEST_ASSERT_EQUAL_UINT32(1024u, pmalloc_sizeof(&pm, m1));
    TEST_ASSERT_TRUE(pm_test_verify(m1, 1024, 9));
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_realloc_rejects_sizes_that_cannot_be_aligned(void) {
    heap(4096);
    void* p = pmalloc_malloc(&pm, 64);
    TEST_ASSERT_NULL(pmalloc_realloc(&pm, p, UINT32_MAX));
    TEST_ASSERT_EQUAL_UINT32(64u, pmalloc_sizeof(&pm, p));
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

void run_realloc_tests(void) {
    UnitySetTestFile(__FILE__);
    RUN_TEST(test_realloc_null_acts_as_malloc);
    RUN_TEST(test_realloc_same_size_returns_the_same_block);
    RUN_TEST(test_realloc_within_the_span_stays_in_place);
    RUN_TEST(test_realloc_shrink_splits_the_tail);
    RUN_TEST(test_realloc_shrink_by_less_than_a_node_keeps_the_span);
    RUN_TEST(test_realloc_shrink_merges_the_tail_with_a_free_neighbour);
    RUN_TEST(test_realloc_grow_into_the_tail_stays_in_place);
    RUN_TEST(test_realloc_grow_into_a_freed_neighbour_stays_in_place);
    RUN_TEST(test_realloc_grow_absorbs_a_neighbour_too_small_to_split);
    RUN_TEST(test_realloc_grow_moves_when_the_next_block_is_assigned);
    RUN_TEST(test_realloc_grow_moves_when_the_free_neighbour_is_too_small);
    RUN_TEST(test_realloc_grow_at_the_end_of_a_region_moves_without_overrun);
    RUN_TEST(test_realloc_fails_without_space_and_keeps_the_block);
    RUN_TEST(test_realloc_rejects_sizes_that_cannot_be_aligned);
}
