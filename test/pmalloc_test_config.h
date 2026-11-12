//
// pmalloc build configuration for the tests
//
// The tests build their own copy of pmalloc.c with this file as PMALLOC_CONFIG_HEADER, so every
// failed check is recorded by pm_test_check_failed in test_support.c.
//

#ifndef PMALLOC_TEST_CONFIG_H
#define PMALLOC_TEST_CONFIG_H

struct pmalloc;
void pm_test_check_failed(struct pmalloc* pm, void* ptr);

#define PMALLOC_CHECK_FAILED(pm, ptr) pm_test_check_failed(pm, ptr)

#endif
