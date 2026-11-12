//
// pmalloc_malloc and pmalloc_sizeof
//
// - every returned pointer is aligned to PMALLOC_ALIGN, wherever the region starts
// - pmalloc_sizeof reports exactly the size asked for, unless the block took a tail too small to
//   hold a header and PMALLOC_ALIGN bytes, when it reports the whole usable size of the block
// - allocation is first fit in address order; a request no free node can hold returns NULL
// - a request that cannot be rounded up to PMALLOC_ALIGN within 32 bits returns NULL
// - allocating into a hole between assigned blocks leaves both chains intact
//

#include "test_support.h"

static pmalloc_t pm;

static void test_malloc_returns_aligned_pointers(void)
{
	void *mem[97];
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena + 3, 65536);
	for (uint32_t size = 0; size <= 96; size++) {
		mem[size] = pmalloc_malloc(&pm, size);
		TEST_ASSERT_NOT_NULL(mem[size]);
		TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)((uintptr_t)mem[size] % PM_TEST_ALIGN));
		pm_test_fill(mem[size], size, size);
	}
	pm_test_check_heap(&pm, 1);
	for (uint32_t size = 0; size <= 96; size++) {
		TEST_ASSERT_TRUE(pm_test_verify(mem[size], size, size));
	}
}

static void test_malloc_reports_the_requested_size(void)
{
	const uint32_t len[8] = {150, 256, 512, 100, 1024, 1, 7, 33};
	void *mem[8];
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 65536);
	for (int i = 0; i < 8; i++) mem[i] = pmalloc_malloc(&pm, len[i]);
	for (int i = 0; i < 8; i++) TEST_ASSERT_EQUAL_UINT32(len[i], pmalloc_sizeof(&pm, mem[i]));
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 65536);
}

static void test_malloc_alloc_size_free(void)
{
	const uint32_t len[6] = {150, 256, 512, 100, 1024, 65536};
	void *mem[6];
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 65536);
	for (int i = 0; i < 6; i++) mem[i] = pmalloc_malloc(&pm, len[i]);
	TEST_ASSERT_NULL_MESSAGE(mem[5], "pmalloc_malloc(65536) allocated when it should not have");
	for (int i = 0; i < 5; i++) TEST_ASSERT_EQUAL_UINT32(len[i], pmalloc_sizeof(&pm, mem[i]));
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 65536);

	for (int i = 0; i < 5; i++) pmalloc_free(&pm, mem[i]);
	TEST_ASSERT_EQUAL_UINT32(1u, pm.totalnodes);
	TEST_ASSERT_EQUAL_UINT32(pmalloc_totalmem(&pm), pmalloc_freemem(&pm));
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 65536);
}

static void test_malloc_fails_on_fragmentation(void)
{
	void *mem[8];
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 8192);
	for (int i = 0; i < 8; i++) TEST_ASSERT_NOT_NULL(mem[i] = pmalloc_malloc(&pm, 512));
	void *rest = pmalloc_malloc(&pm, pmalloc_freemem(&pm));
	TEST_ASSERT_NOT_NULL(rest);
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_freemem(&pm));

	pmalloc_free(&pm, mem[1]);
	pmalloc_free(&pm, mem[3]);
	pmalloc_free(&pm, mem[5]);
	TEST_ASSERT_NULL_MESSAGE(pmalloc_malloc(&pm, 1024), "pmalloc_malloc should fail because of fragmentation");
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 8192);

	pmalloc_free(&pm, mem[0]);
	pmalloc_free(&pm, mem[2]);
	pmalloc_free(&pm, mem[4]);
	pmalloc_free(&pm, mem[6]);
	pmalloc_free(&pm, mem[7]);
	pmalloc_free(&pm, rest);
	TEST_ASSERT_EQUAL_UINT32(1u, pm.totalnodes);
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 8192);
}

static void test_malloc_exact_fit_takes_the_whole_node(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 4096);
	uint32_t free_bytes = pmalloc_freemem(&pm);
	void *p = pmalloc_malloc(&pm, free_bytes);
	TEST_ASSERT_EQUAL_PTR(pm_test_arena + PM_TEST_HDR, p);
	TEST_ASSERT_EQUAL_UINT32(free_bytes, pmalloc_sizeof(&pm, p));
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_freemem(&pm));
	TEST_ASSERT_NULL(pm.available);
	TEST_ASSERT_NULL(pmalloc_malloc(&pm, 0));
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_malloc_absorbs_a_tail_too_small_to_track(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 4096);
	uint32_t free_bytes = pmalloc_freemem(&pm);
	void *p = pmalloc_malloc(&pm, free_bytes - PM_TEST_HDR);
	TEST_ASSERT_NOT_NULL(p);
	TEST_ASSERT_EQUAL_UINT32(free_bytes, pmalloc_sizeof(&pm, p));
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_freemem(&pm));
	TEST_ASSERT_EQUAL_UINT32(1u, pm.totalnodes);
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);

	pmalloc_free(&pm, p);
	p = pmalloc_malloc(&pm, free_bytes - PM_TEST_HDR - PM_TEST_ALIGN);
	TEST_ASSERT_EQUAL_UINT32(free_bytes - PM_TEST_HDR - PM_TEST_ALIGN, pmalloc_sizeof(&pm, p));
	TEST_ASSERT_EQUAL_UINT32(PM_TEST_ALIGN, pmalloc_freemem(&pm));
	TEST_ASSERT_EQUAL_UINT32(2u, pm.totalnodes);
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_malloc_into_a_hole_keeps_both_chains(void)
{
	void *mem[4];
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 65536);
	for (int i = 0; i < 4; i++) mem[i] = pmalloc_malloc(&pm, 200);
	pmalloc_free(&pm, mem[1]);
	TEST_ASSERT_EQUAL_UINT32(3u, pm_test_count(pm.assigned));

	void *hole = pmalloc_malloc(&pm, 64);
	TEST_ASSERT_EQUAL_PTR(mem[1], hole);
	TEST_ASSERT_EQUAL_UINT32(4u, pm_test_count(pm.assigned));
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 65536);

	pmalloc_free(&pm, mem[0]);
	pmalloc_free(&pm, hole);
	pmalloc_free(&pm, mem[2]);
	pmalloc_free(&pm, mem[3]);
	TEST_ASSERT_EQUAL_UINT32(1u, pm.totalnodes);
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 65536);
}

static void test_malloc_first_fit_skips_small_nodes(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 65536);
	void *a = pmalloc_malloc(&pm, 64);
	void *b = pmalloc_malloc(&pm, 64);
	void *c = pmalloc_malloc(&pm, 512);
	void *d = pmalloc_malloc(&pm, 64);
	pmalloc_free(&pm, a);
	pmalloc_free(&pm, c);
	TEST_ASSERT_EQUAL_PTR(c, pmalloc_malloc(&pm, 300));
	TEST_ASSERT_NOT_NULL(b);
	TEST_ASSERT_NOT_NULL(d);
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 65536);
}

static void test_malloc_rejects_sizes_that_cannot_be_aligned(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 4096);
	uint32_t free_bytes = pmalloc_freemem(&pm);
	TEST_ASSERT_NULL(pmalloc_malloc(&pm, UINT32_MAX));
	TEST_ASSERT_NULL(pmalloc_malloc(&pm, UINT32_MAX - PM_TEST_ALIGN + 2u));
	TEST_ASSERT_NULL(pmalloc_malloc(&pm, UINT32_MAX - PM_TEST_ALIGN + 1u));
	TEST_ASSERT_EQUAL_UINT32(free_bytes, pmalloc_freemem(&pm));
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_malloc_zero_bytes(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 4096);
	void *p = pmalloc_malloc(&pm, 0);
	void *q = pmalloc_malloc(&pm, 0);
	TEST_ASSERT_NOT_NULL(p);
	TEST_ASSERT_NOT_NULL(q);
	TEST_ASSERT_TRUE(p != q);
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_sizeof(&pm, p));
	pmalloc_free(&pm, p);
	pmalloc_free(&pm, q);
	TEST_ASSERT_EQUAL_UINT32(1u, pm.totalnodes);
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_malloc_without_memory_returns_null(void)
{
	pmalloc_init(&pm);
	TEST_ASSERT_NULL(pmalloc_malloc(&pm, 1));
	TEST_ASSERT_NULL(pmalloc_malloc(&pm, 0));
}

void run_malloc_tests(void)
{
	UnitySetTestFile(__FILE__);
	RUN_TEST(test_malloc_returns_aligned_pointers);
	RUN_TEST(test_malloc_reports_the_requested_size);
	RUN_TEST(test_malloc_alloc_size_free);
	RUN_TEST(test_malloc_fails_on_fragmentation);
	RUN_TEST(test_malloc_exact_fit_takes_the_whole_node);
	RUN_TEST(test_malloc_absorbs_a_tail_too_small_to_track);
	RUN_TEST(test_malloc_into_a_hole_keeps_both_chains);
	RUN_TEST(test_malloc_first_fit_skips_small_nodes);
	RUN_TEST(test_malloc_rejects_sizes_that_cannot_be_aligned);
	RUN_TEST(test_malloc_zero_bytes);
	RUN_TEST(test_malloc_without_memory_returns_null);
}
