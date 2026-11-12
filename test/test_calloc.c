//
// pmalloc_calloc
//
// - the block holds num * size bytes, all zero, even when the memory was used before
// - num * size overflowing 32 bits returns NULL and allocates nothing
// - a zero count or zero size returns a zero-sized block
// - a request no free node can hold returns NULL
//

#include "test_support.h"

static pmalloc_t pm;

static void test_calloc_zeroes_reused_memory(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 4096);
	unsigned char *dirty = pmalloc_malloc(&pm, 256);
	for (int i = 0; i < 256; i++) dirty[i] = 0xAA;
	pmalloc_free(&pm, dirty);

	unsigned char *clean = pmalloc_calloc(&pm, 16, 16);
	TEST_ASSERT_EQUAL_PTR(dirty, clean);
	TEST_ASSERT_EQUAL_UINT32(256u, pmalloc_sizeof(&pm, clean));
	for (int i = 0; i < 256; i++) TEST_ASSERT_EQUAL_HEX8(0x00, clean[i]);
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_calloc_rejects_an_overflowing_product(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 4096);
	uint32_t free_bytes = pmalloc_freemem(&pm);
	TEST_ASSERT_NULL(pmalloc_calloc(&pm, 65536, 65537));
	TEST_ASSERT_NULL(pmalloc_calloc(&pm, UINT32_MAX, 2));
	TEST_ASSERT_NULL(pmalloc_calloc(&pm, 2, UINT32_MAX));
	TEST_ASSERT_EQUAL_UINT32(free_bytes, pmalloc_freemem(&pm));
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_calloc_zero_count_or_size(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 4096);
	void *a = pmalloc_calloc(&pm, 5, 0);
	void *b = pmalloc_calloc(&pm, 0, 5);
	TEST_ASSERT_NOT_NULL(a);
	TEST_ASSERT_NOT_NULL(b);
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_sizeof(&pm, a));
	TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_sizeof(&pm, b));
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_calloc_out_of_memory(void)
{
	pmalloc_init(&pm);
	pmalloc_addblock(&pm, pm_test_arena, 4096);
	TEST_ASSERT_NULL(pmalloc_calloc(&pm, 1024, 1024));
	pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

void run_calloc_tests(void)
{
	UnitySetTestFile(__FILE__);
	RUN_TEST(test_calloc_zeroes_reused_memory);
	RUN_TEST(test_calloc_rejects_an_overflowing_product);
	RUN_TEST(test_calloc_zero_count_or_size);
	RUN_TEST(test_calloc_out_of_memory);
}
