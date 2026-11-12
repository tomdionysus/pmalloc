//
// Randomised operation sequences
//
// Runs long, seeded sequences of malloc, calloc, realloc and free over one region and over several
// (two of them touching), and after every operation proves:
//
// - every heap invariant in pm_test_check_heap
// - every live block still holds the bytes written to it, and a moved block kept its contents
// - calloc memory is zero, and every pointer is aligned to PMALLOC_ALIGN
// - pmalloc_sizeof is at least the size asked for, and short of it by less than a header and two
//   alignment units
// - nothing outside the regions is written, and freeing everything restores one node per region
//

#include "test_support.h"

#define SLOTS 256u

typedef struct {
    void* ptr;
    uint32_t size;
    uint32_t seed;
} slot_t;

static pmalloc_t pm;
static slot_t slots[SLOTS];
static uint32_t rng;
static uint32_t generation;

static uint32_t rnd(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static uint32_t rnd_size(void) {
    uint32_t pick = rnd() % 100u;
    if (pick < 60u) return rnd() % 64u;
    if (pick < 90u) return rnd() % 512u;
    return rnd() % 4096u;
}

static void check_slot(const slot_t* s) {
    TEST_ASSERT_TRUE_MESSAGE(pm_test_verify(s->ptr, s->size, s->seed), "block contents changed");
    uint32_t usable = pmalloc_sizeof(&pm, s->ptr);
    TEST_ASSERT_TRUE_MESSAGE(usable >= s->size,
                             "pmalloc_sizeof is smaller than the size asked for");
    TEST_ASSERT_TRUE_MESSAGE(usable < s->size + PM_TEST_HDR + 2u * PM_TEST_ALIGN,
                             "pmalloc_sizeof is too far above the size asked for");
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)((uintptr_t)s->ptr % PM_TEST_ALIGN));
}

static void keep(slot_t* s, void* ptr, uint32_t size) {
    s->ptr = ptr;
    s->size = size;
    s->seed = ++generation;
    pm_test_fill(ptr, size, s->seed);
}

static void stress(uint32_t seed, uint32_t ops, uint32_t regions) {
    rng = seed;
    for (uint32_t i = 0; i < SLOTS; i++) slots[i].ptr = NULL;

    for (uint32_t op = 0; op < ops; op++) {
        slot_t* s = &slots[rnd() % SLOTS];
        uint32_t action = rnd() % 4u;
        if (s->ptr == NULL) {
            uint32_t size = rnd_size();
            unsigned char* p =
                action == 0u ? pmalloc_calloc(&pm, size, 1) : pmalloc_malloc(&pm, size);
            if (p != NULL) {
                for (uint32_t i = 0; action == 0u && i < size; i++)
                    TEST_ASSERT_EQUAL_HEX8(0x00, p[i]);
                keep(s, p, size);
            }
        } else if (action < 2u) {
            check_slot(s);
            pmalloc_free(&pm, s->ptr);
            s->ptr = NULL;
        } else {
            check_slot(s);
            uint32_t size = rnd_size();
            void* p = pmalloc_realloc(&pm, s->ptr, size);
            if (p != NULL) {
                uint32_t kept = size < s->size ? size : s->size;
                TEST_ASSERT_TRUE_MESSAGE(pm_test_verify(p, kept, s->seed),
                                         "realloc lost the block contents");
                keep(s, p, size);
            } else {
                check_slot(s);
            }
        }
        pm_test_check_heap(&pm, regions);
    }

    for (uint32_t i = 0; i < SLOTS; i++) {
        if (slots[i].ptr == NULL) continue;
        check_slot(&slots[i]);
        pmalloc_free(&pm, slots[i].ptr);
    }
    pm_test_check_heap(&pm, regions);
    TEST_ASSERT_EQUAL_UINT32(regions, pm.totalnodes);
    TEST_ASSERT_EQUAL_UINT32(0u, pmalloc_usedmem(&pm));
}

static void test_stress_one_region(void) {
    const uint32_t seeds[3] = {1u, 0x9E3779B9u, 0xC0FFEEu};
    for (int i = 0; i < 3; i++) {
        pmalloc_init(&pm);
        pmalloc_addblock(&pm, pm_test_arena + 5, 65536);
        stress(seeds[i], 20000, 1);
        pm_test_check_poison(pm_test_arena + 5 + 65536, 4096);
    }
}

static void test_stress_three_regions(void) {
    pmalloc_init(&pm);
    pmalloc_addblock(&pm, pm_test_arena, 16384);
    pmalloc_addblock(&pm, pm_test_arena + 16384, 16384);
    pmalloc_addblock(&pm, pm_test_arena + 65536 + 3, 20000);
    stress(0xDEADBEEFu, 30000, 3);
    pm_test_check_poison(pm_test_arena + 65536 + 3 + 20000, 4096);
}

void run_stress_tests(void) {
    UnitySetTestFile(__FILE__);
    RUN_TEST(test_stress_one_region);
    RUN_TEST(test_stress_three_regions);
}
