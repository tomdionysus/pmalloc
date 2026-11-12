//
// pmalloc test support
//
// The heap checker and helpers shared by every test file. pm_test_check_heap proves the
// structural invariants of a pmalloc_t after any sequence of calls:
//
//   - both chains are doubly linked, NULL-terminated and strictly ascending by address
//   - every node and every returned pointer is aligned to PMALLOC_ALIGN
//   - no two nodes overlap, and no two free nodes are adjacent (free space is fully merged)
//   - freemem is the sum of the free nodes' sizes, and never exceeds totalmem
//   - totalnodes counts every node, and overheadmem is totalnodes headers
//   - totalmem equals the bytes in every node plus the headers created by splitting
//
// pm_test_check_tiling adds, for a heap built from one region, that the nodes cover the region
// exactly with no gaps.
//

#ifndef PMALLOC_TEST_SUPPORT_H
#define PMALLOC_TEST_SUPPORT_H

#include <stdint.h>

#include "pmalloc.h"
#include "unity.h"

#define PM_TEST_ALIGN ((uint32_t)PMALLOC_ALIGN)
#define PM_TEST_HDR \
    (((uint32_t)sizeof(pmalloc_item_t) + PM_TEST_ALIGN - 1u) & ~(PM_TEST_ALIGN - 1u))

// The arena every test carves its regions from. setUp fills it with PM_TEST_POISON so a test
// can prove that bytes outside its region were never written.
#define PM_TEST_ARENA_BYTES 262144u
#define PM_TEST_POISON 0x5Au
extern unsigned char pm_test_arena[PM_TEST_ARENA_BYTES];

uint32_t pm_test_align_up(uint32_t n);
uint32_t pm_test_count(const pmalloc_item_t* list);

void pm_test_check_heap(pmalloc_t* pm, uint32_t regions);
void pm_test_check_tiling(pmalloc_t* pm, const void* start, const void* end);
void pm_test_check_poison(const unsigned char* from, uint32_t bytes);

// Every PMALLOC_CHECK_FAILED call: how many since setUp, and the arguments of the last one.
// tearDown fails a test that leaves this non-zero, so a test expecting failures resets it.
extern uint32_t pm_test_check_failures;
extern pmalloc_t* pm_test_check_last_pm;
extern void* pm_test_check_last_ptr;

void pm_test_fill(void* ptr, uint32_t bytes, uint32_t seed);
int pm_test_verify(const void* ptr, uint32_t bytes, uint32_t seed);

void run_addblock_tests(void);
void run_malloc_tests(void);
void run_free_tests(void);
void run_calloc_tests(void);
void run_realloc_tests(void);
void run_internal_tests(void);
void run_check_tests(void);
void run_stress_tests(void);

#endif
