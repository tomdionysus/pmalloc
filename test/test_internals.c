//
// Chain primitives, statistics and version
//
// - pmalloc_item_insert keeps a chain ascending by address and sets both links of the inserted
//   node, whatever they held before
// - pmalloc_item_remove unlinks a head, middle or tail node, moves the root, and clears the
//   removed node's links
// - pmalloc_merge folds a run of adjacent free nodes into one, returning each folded header to
//   freemem
// - pmalloc_sizeof(NULL) is 0; usedmem is every assigned span plus the headers made by splitting
// - PMALLOC_VERSION spells out the three version numbers
// - pmalloc_dump_stats prints both chains in a DEBUG build
//

#include <stdio.h>
#include <string.h>

#include "test_support.h"

static pmalloc_t pm;

static void test_item_insert_keeps_order_and_sets_links(void)
{
	pmalloc_item_t nodes[5];
	pmalloc_item_t *root = NULL;
	memset(nodes, 0xA5, sizeof nodes);
	const int order[5] = {2, 0, 4, 1, 3};
	for (int i = 0; i < 5; i++) pmalloc_item_insert(&root, &nodes[order[i]]);

	TEST_ASSERT_EQUAL_PTR(&nodes[0], root);
	for (int i = 0; i < 5; i++) {
		TEST_ASSERT_EQUAL_PTR(i == 0 ? NULL : &nodes[i - 1], nodes[i].prev);
		TEST_ASSERT_EQUAL_PTR(i == 4 ? NULL : &nodes[i + 1], nodes[i].next);
	}
}

static void test_item_remove_head_middle_and_tail(void)
{
	pmalloc_item_t nodes[5];
	pmalloc_item_t *root = NULL;
	for (int i = 0; i < 5; i++) pmalloc_item_insert(&root, &nodes[i]);

	pmalloc_item_remove(&root, &nodes[0]);
	TEST_ASSERT_EQUAL_PTR(&nodes[1], root);
	TEST_ASSERT_NULL(nodes[1].prev);
	pmalloc_item_remove(&root, &nodes[2]);
	TEST_ASSERT_EQUAL_PTR(&nodes[3], nodes[1].next);
	TEST_ASSERT_EQUAL_PTR(&nodes[1], nodes[3].prev);
	pmalloc_item_remove(&root, &nodes[4]);
	TEST_ASSERT_NULL(nodes[3].next);
	for (int i = 0; i < 5; i += 2) {
		TEST_ASSERT_NULL(nodes[i].prev);
		TEST_ASSERT_NULL(nodes[i].next);
	}

	pmalloc_item_remove(&root, &nodes[3]);
	pmalloc_item_remove(&root, &nodes[1]);
	TEST_ASSERT_NULL(root);
}

static void test_merge_folds_a_run_of_free_nodes(void)
{
	pmalloc_init(&pm);
	unsigned char *at = pm_test_arena;
	for (int i = 0; i < 3; i++) {
		pmalloc_item_t *node = (pmalloc_item_t *)at;
		node->size = 64;
		pmalloc_item_insert(&pm.available, node);
		pm.freemem += 64;
		pm.totalnodes++;
		at += PM_TEST_HDR + 64;
	}
	pm.totalmem = 3u * 64u + 2u * PM_TEST_HDR;

	pmalloc_merge(&pm, (pmalloc_item_t *)(pm_test_arena + PM_TEST_HDR + 64));
	TEST_ASSERT_EQUAL_UINT32(1u, pm.totalnodes);
	TEST_ASSERT_EQUAL_UINT32(3u * 64u + 2u * PM_TEST_HDR, pm.available->size);
	TEST_ASSERT_EQUAL_UINT32(pm.totalmem, pmalloc_freemem(&pm));
	pm_test_check_tiling(&pm, pm_test_arena, at);
}

static void test_sizeof_null_is_zero(void)
{
	pmalloc_init(&pm);
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_sizeof(&pm, NULL));
}

static void test_statistics_track_allocations(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 4096);
	TEST_ASSERT_EQUAL_UINT32(4096u - PM_TEST_HDR, pmalloc_totalmem(&pm));
	void *a = pmalloc_malloc(&pm, 100);
	void *b = pmalloc_malloc(&pm, 3);
	uint32_t used = 2u * PM_TEST_HDR + pm_test_align_up(100) + pm_test_align_up(3);
	TEST_ASSERT_EQUAL_UINT32(used, pmalloc_usedmem(&pm));
	TEST_ASSERT_EQUAL_UINT32(4096u - PM_TEST_HDR - used, pmalloc_freemem(&pm));
	TEST_ASSERT_EQUAL_UINT32(3u * PM_TEST_HDR, pmalloc_overheadmem(&pm));
	TEST_ASSERT_EQUAL_UINT32(4096u - PM_TEST_HDR, pmalloc_totalmem(&pm));
	pmalloc_free(&pm, a);
	pmalloc_free(&pm, b);
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_usedmem(&pm));
	TEST_ASSERT_EQUAL_UINT32(PM_TEST_HDR, pmalloc_overheadmem(&pm));
}

static void test_version_macros_agree(void)
{
	char expected[32];
	snprintf(expected, sizeof expected, "%d.%d.%d", PMALLOC_VERSION_MAJOR, PMALLOC_VERSION_MINOR, PMALLOC_VERSION_PATCH);
	TEST_ASSERT_EQUAL_STRING(expected, PMALLOC_VERSION);
}

static void test_dump_stats_prints_both_chains(void)
{
#ifdef DEBUG
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 4096);
	void *a = pmalloc_malloc(&pm, 100);
	void *b = pmalloc_malloc(&pm, 100);
	pmalloc_free(&pm, a);
	pmalloc_dump_stats(&pm);
	TEST_ASSERT_NOT_NULL(b);
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
#else
	TEST_IGNORE_MESSAGE("pmalloc_dump_stats exists only in a DEBUG build");
#endif
}

void run_internal_tests(void)
{
	UnitySetTestFile(__FILE__);
	RUN_TEST(test_item_insert_keeps_order_and_sets_links);
	RUN_TEST(test_item_remove_head_middle_and_tail);
	RUN_TEST(test_merge_folds_a_run_of_free_nodes);
	RUN_TEST(test_sizeof_null_is_zero);
	RUN_TEST(test_statistics_track_allocations);
	RUN_TEST(test_version_macros_agree);
	RUN_TEST(test_dump_stats_prints_both_chains);
}
