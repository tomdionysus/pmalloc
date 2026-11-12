//
// PMALLOC_CHECKS
//
// With PMALLOC_CHECKS defined:
//
// - freeing a block twice, or a pointer pmalloc did not return (a stack address, a misaligned or
//   interior pointer, an address in free memory), changes nothing and calls PMALLOC_CHECK_FAILED
//   with the heap and the pointer
// - pmalloc_realloc of such a pointer returns NULL, and pmalloc_sizeof returns 0
// - a region overlapping any block, free or assigned, is ignored; a region that only touches the
//   heap is accepted
// - NULL is never a failure
//
// Without PMALLOC_CHECKS these tests are ignored. In a checked build, tearDown also proves every
// other test's correct use never fails a check.
//

#include "test_support.h"

#ifdef PMALLOC_CHECKS
#define REQUIRE_CHECKS() ((void)0)
#else
#define REQUIRE_CHECKS() TEST_IGNORE_MESSAGE("needs a PMALLOC_CHECKS build")
#endif

static pmalloc_t pm;

static void heap(uint32_t bytes) {
    pmalloc_init(&pm);
    pmalloc_addblock(&pm, pm_test_arena, bytes);
}

static void expect_failure(const void* ptr) {
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, pm_test_check_failures, "expected one failed check");
    TEST_ASSERT_EQUAL_PTR(&pm, pm_test_check_last_pm);
    TEST_ASSERT_EQUAL_PTR(ptr, pm_test_check_last_ptr);
    pm_test_check_failures = 0;
}

static void test_checks_ignore_a_double_free(void) {
    REQUIRE_CHECKS();
    heap(4096);
    void* p = pmalloc_malloc(&pm, 64);
    void* q = pmalloc_malloc(&pm, 64);
    pmalloc_free(&pm, p);
    uint32_t free_bytes = pmalloc_freemem(&pm);
    uint32_t nodes = pm.totalnodes;

    pmalloc_free(&pm, p);
    expect_failure(p);
    TEST_ASSERT_EQUAL_UINT32(free_bytes, pmalloc_freemem(&pm));
    TEST_ASSERT_EQUAL_UINT32(nodes, pm.totalnodes);
    TEST_ASSERT_EQUAL_UINT32(64u, pmalloc_sizeof(&pm, q));
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_checks_ignore_pointers_the_heap_did_not_return(void) {
    REQUIRE_CHECKS();
    heap(4096);
    unsigned char* p = pmalloc_malloc(&pm, 64);
    unsigned char local = 0;
    void* bad[4] = {&local, p + 1, p + PM_TEST_ALIGN, pm_test_arena + 2048};
    for (int i = 0; i < 4; i++) {
        pmalloc_free(&pm, bad[i]);
        expect_failure(bad[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(64u, pmalloc_sizeof(&pm, p));
    TEST_ASSERT_EQUAL_UINT8(0u, local);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_checks_refuse_realloc_of_a_bad_pointer(void) {
    REQUIRE_CHECKS();
    heap(4096);
    void* p = pmalloc_malloc(&pm, 64);
    pmalloc_free(&pm, p);
    uint32_t free_bytes = pmalloc_freemem(&pm);

    TEST_ASSERT_NULL(pmalloc_realloc(&pm, p, 128));
    expect_failure(p);
    TEST_ASSERT_EQUAL_UINT32(free_bytes, pmalloc_freemem(&pm));
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

static void test_checks_sizeof_a_bad_pointer_is_zero(void) {
    REQUIRE_CHECKS();
    heap(4096);
    unsigned char* p = pmalloc_malloc(&pm, 64);
    TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_sizeof(&pm, p + PM_TEST_ALIGN));
    expect_failure(p + PM_TEST_ALIGN);
    pmalloc_free(&pm, p);
    TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_sizeof(&pm, p));
    expect_failure(p);
}

static void test_checks_refuse_an_overlapping_region(void) {
    REQUIRE_CHECKS();
    heap(4096);
    uint32_t total = pmalloc_totalmem(&pm);

    pmalloc_addblock(&pm, pm_test_arena, 4096);
    expect_failure(pm_test_arena);
    pmalloc_addblock(&pm, pm_test_arena + 2048, 4096);
    expect_failure(pm_test_arena + 2048);

    void* p = pmalloc_malloc(&pm, 64);
    pmalloc_addblock(&pm, p, 32);
    expect_failure(p);
    TEST_ASSERT_EQUAL_UINT32(total, pmalloc_totalmem(&pm));
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);

    pmalloc_init(&pm);
    pmalloc_addblock(&pm, pm_test_arena + 1024, 1024);
    pmalloc_addblock(&pm, pm_test_arena + 512, 1024);
    expect_failure(pm_test_arena + 512);
    pmalloc_addblock(&pm, pm_test_arena, 1024);
    TEST_ASSERT_EQUAL_UINT32(0u, pm_test_check_failures);
    TEST_ASSERT_EQUAL_UINT32(2u, pm.totalnodes);
    pm_test_check_heap(&pm, 2);
}

static void test_checks_null_is_never_a_failure(void) {
    REQUIRE_CHECKS();
    heap(4096);
    pmalloc_free(&pm, NULL);
    TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_sizeof(&pm, NULL));
    void* p = pmalloc_realloc(&pm, NULL, 16);
    TEST_ASSERT_NOT_NULL(p);
    pmalloc_addblock(&pm, NULL, 1024);
    TEST_ASSERT_EQUAL_UINT32(0u, pm_test_check_failures);
    pm_test_check_tiling(&pm, pm_test_arena, pm_test_arena + 4096);
}

void run_check_tests(void) {
    UnitySetTestFile(__FILE__);
    RUN_TEST(test_checks_ignore_a_double_free);
    RUN_TEST(test_checks_ignore_pointers_the_heap_did_not_return);
    RUN_TEST(test_checks_refuse_realloc_of_a_bad_pointer);
    RUN_TEST(test_checks_sizeof_a_bad_pointer_is_zero);
    RUN_TEST(test_checks_refuse_an_overlapping_region);
    RUN_TEST(test_checks_null_is_never_a_failure);
}
