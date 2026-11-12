//
// pmalloc test runner
//
// One binary, pmalloc_tests, runs every test file in turn. setUp poisons the shared arena before
// each test so that writes outside a test's region are detectable, and clears the record of failed
// checks. tearDown fails any test that tripped a check it did not expect, so in a PMALLOC_CHECKS
// build every test also proves that correct use never fails a check.
//

#include <string.h>

#include "test_support.h"

void setUp(void) {
    memset(pm_test_arena, PM_TEST_POISON, sizeof(pm_test_arena));
    pm_test_check_failures = 0;
    pm_test_check_last_pm = NULL;
    pm_test_check_last_ptr = NULL;
}

void tearDown(void) {
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, pm_test_check_failures, "an unexpected check failed");
}

int main(void) {
    UNITY_BEGIN();
    run_addblock_tests();
    run_malloc_tests();
    run_free_tests();
    run_calloc_tests();
    run_realloc_tests();
    run_internal_tests();
    run_check_tests();
    run_stress_tests();
    return UNITY_END();
}
