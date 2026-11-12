//
// pmalloc test runner
//
// One binary, pmalloc_tests, runs every test file in turn. setUp poisons the shared arena before
// each test so that writes outside a test's region are detectable.
//

#include <string.h>

#include "test_support.h"

void setUp(void) { memset(pm_test_arena, PM_TEST_POISON, sizeof(pm_test_arena)); }

void tearDown(void) {}

int main(void)
{
	UNITY_BEGIN();
	run_addblock_tests();
	run_malloc_tests();
	run_free_tests();
	run_calloc_tests();
	run_realloc_tests();
	run_internal_tests();
	run_stress_tests();
	return UNITY_END();
}
