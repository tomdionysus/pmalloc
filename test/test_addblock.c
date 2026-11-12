//
// pmalloc_init and pmalloc_addblock
//
// - init clears every field of the pmalloc_t
// - a region becomes one free node at its first aligned address, covering it to its last aligned
//   address; totalmem and freemem are its bytes less one header
// - a NULL region, or one too small for a header and PMALLOC_ALIGN bytes, is ignored
// - regions accumulate: totalmem is the sum of the regions, not a running total of freemem
// - a region touching an existing node, free or assigned, before or after it, is kept PMALLOC_ALIGN
//   bytes apart from it, so regions are never merged and the accounting holds after every free
//

#include <string.h>

#include "test_support.h"

static pmalloc_t pm;

static uintptr_t align_up_ptr(const void *p) { return ((uintptr_t)p + PM_TEST_ALIGN - 1u) & ~(uintptr_t)(PM_TEST_ALIGN - 1u); }
static uintptr_t align_down_ptr(const void *p) { return (uintptr_t)p & ~(uintptr_t)(PM_TEST_ALIGN - 1u); }

static void test_init_clears_every_field(void)
{
	memset(&pm, 0xA5, sizeof pm);
	pmalloc_init(&pm);
	TEST_ASSERT_NULL(pm.available);
	TEST_ASSERT_NULL(pm.assigned);
	TEST_ASSERT_EQUAL_UINT32(0u, pm.freemem);
	TEST_ASSERT_EQUAL_UINT32(0u, pm.totalmem);
	TEST_ASSERT_EQUAL_UINT32(0u, pm.totalnodes);
}

static void test_addblock_makes_one_free_node(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 4096);
	TEST_ASSERT_EQUAL_PTR(pm_test_arena, pm.available);
	TEST_ASSERT_NULL(pm.assigned);
	TEST_ASSERT_EQUAL_UINT32(4096u - PM_TEST_HDR, pmalloc_totalmem(&pm));
	TEST_ASSERT_EQUAL_UINT32(4096u - PM_TEST_HDR, pmalloc_freemem(&pm));
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_usedmem(&pm));
	TEST_ASSERT_EQUAL_UINT32(PM_TEST_HDR, pmalloc_overheadmem(&pm));
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_addblock_aligns_a_misaligned_region(void)
{
	unsigned char *raw = pm_test_arena + 3;
	uintptr_t start = align_up_ptr(raw);
	uintptr_t end = align_down_ptr(raw + 4096);
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, raw, 4096);
	TEST_ASSERT_EQUAL_PTR((void *)start, pm.available);
	TEST_ASSERT_EQUAL_UINT32((uint32_t)(end - start) - PM_TEST_HDR, pmalloc_totalmem(&pm));
	pm_test_check_tiling(&pm, (void *)start, (void *)end);
	pm_test_check_poison(pm_test_arena, (uint32_t)(start - (uintptr_t)pm_test_arena));
}

static void test_addblock_ignores_null(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, NULL, 4096);
	TEST_ASSERT_NULL(pm.available);
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_totalmem(&pm));
	TEST_ASSERT_NULL(pmalloc_malloc(&pm, 1));
}

static void test_addblock_ignores_a_region_too_small(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, PM_TEST_HDR);
	pmalloc_addblock(&pm, pm_test_arena + 1, 2);
	TEST_ASSERT_NULL(pm.available);
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_totalmem(&pm));
	TEST_ASSERT_EQUAL_UINT32(0u, pm.totalnodes);
	pm_test_check_poison(pm_test_arena, 64);

	pmalloc_addblock(&pm, pm_test_arena, PM_TEST_HDR + PM_TEST_ALIGN);
	TEST_ASSERT_EQUAL_UINT32(PM_TEST_ALIGN, pmalloc_totalmem(&pm));
	pm_test_check_heap(&pm, 1);
}

static void test_addblock_regions_accumulate(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 4096);
	pmalloc_addblock(&pm, pm_test_arena + 8192, 2048);
	uint32_t total = (4096u - PM_TEST_HDR) + (2048u - PM_TEST_HDR);
	TEST_ASSERT_EQUAL_UINT32(total, pmalloc_totalmem(&pm));
	TEST_ASSERT_EQUAL_UINT32(total, pmalloc_freemem(&pm));
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_usedmem(&pm));
	pm_test_check_heap(&pm, 2);

	void *p = pmalloc_malloc(&pm, 3000);
	void *q = pmalloc_malloc(&pm, 1500);
	TEST_ASSERT_NOT_NULL(p);
	TEST_ASSERT_NOT_NULL(q);
	TEST_ASSERT_TRUE((unsigned char *)q >= pm_test_arena + 8192);
	pm_test_check_heap(&pm, 2);

	pmalloc_free(&pm, p);
	pmalloc_free(&pm, q);
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_usedmem(&pm));
	TEST_ASSERT_EQUAL_UINT32(total, pmalloc_freemem(&pm));
	pm_test_check_heap(&pm, 2);
}

static void test_addblock_keeps_a_gap_after_a_free_node(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 1024);
	pmalloc_addblock(&pm, pm_test_arena + 1024, 1024);
	TEST_ASSERT_EQUAL_PTR(pm_test_arena + 1024 + PM_TEST_ALIGN, pm.available->next);
	TEST_ASSERT_EQUAL_UINT32((1024u - PM_TEST_HDR) + (1024u - PM_TEST_ALIGN - PM_TEST_HDR), pmalloc_totalmem(&pm));
	pm_test_check_heap(&pm, 2);

	void *p = pmalloc_malloc(&pm, 1024u - PM_TEST_HDR);
	void *q = pmalloc_malloc(&pm, 1024u - PM_TEST_ALIGN - PM_TEST_HDR);
	TEST_ASSERT_NOT_NULL(p);
	TEST_ASSERT_NOT_NULL(q);
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_freemem(&pm));
	pmalloc_free(&pm, p);
	pmalloc_free(&pm, q);
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_usedmem(&pm));
	TEST_ASSERT_EQUAL_UINT32(2u, pm.totalnodes);
	pm_test_check_heap(&pm, 2);
}

static void test_addblock_keeps_a_gap_after_an_assigned_node(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 1024);
	void *p = pmalloc_malloc(&pm, 1024u - PM_TEST_HDR);
	TEST_ASSERT_NOT_NULL(p);
	pmalloc_addblock(&pm, pm_test_arena + 1024, 1024);
	TEST_ASSERT_EQUAL_PTR(pm_test_arena + 1024 + PM_TEST_ALIGN, pm.available);

	pmalloc_free(&pm, p);
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_usedmem(&pm));
	TEST_ASSERT_EQUAL_UINT32(2u, pm.totalnodes);
	pm_test_check_heap(&pm, 2);
}

static void test_addblock_keeps_a_gap_before_an_existing_node(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena + 1024, 1024);
	pmalloc_addblock(&pm, pm_test_arena, 1024);
	TEST_ASSERT_EQUAL_PTR(pm_test_arena, pm.available);
	TEST_ASSERT_EQUAL_UINT32(1024u - PM_TEST_ALIGN - PM_TEST_HDR, pm.available->size);
	pm_test_check_heap(&pm, 2);

	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena + 1024, 1024);
	void *p = pmalloc_malloc(&pm, 1024u - PM_TEST_HDR);
	TEST_ASSERT_NOT_NULL(p);
	pmalloc_addblock(&pm, pm_test_arena, 1024);
	TEST_ASSERT_EQUAL_UINT32(1024u - PM_TEST_ALIGN - PM_TEST_HDR, pm.available->size);
	pmalloc_free(&pm, p);
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_usedmem(&pm));
	pm_test_check_heap(&pm, 2);
}

void run_addblock_tests(void)
{
	UnitySetTestFile(__FILE__);
	RUN_TEST(test_init_clears_every_field);
	RUN_TEST(test_addblock_makes_one_free_node);
	RUN_TEST(test_addblock_aligns_a_misaligned_region);
	RUN_TEST(test_addblock_ignores_null);
	RUN_TEST(test_addblock_ignores_a_region_too_small);
	RUN_TEST(test_addblock_regions_accumulate);
	RUN_TEST(test_addblock_keeps_a_gap_after_a_free_node);
	RUN_TEST(test_addblock_keeps_a_gap_after_an_assigned_node);
	RUN_TEST(test_addblock_keeps_a_gap_before_an_existing_node);
}
