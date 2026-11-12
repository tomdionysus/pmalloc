//
// pmalloc test support
//

#include "test_support.h"

#include <stddef.h>

_Alignas(256) unsigned char pm_test_arena[PM_TEST_ARENA_BYTES];

#define PM_TEST_MAX_NODES 32768u

static const pmalloc_item_t *pm_test_nodes[PM_TEST_MAX_NODES];
static int pm_test_node_free[PM_TEST_MAX_NODES];

uint32_t pm_test_align_up(uint32_t n) { return (n + PM_TEST_ALIGN - 1u) & ~(PM_TEST_ALIGN - 1u); }

uint32_t pm_test_count(const pmalloc_item_t *list)
{
	uint32_t count = 0;
	for (; list != NULL; list = list->next) count++;
	return count;
}

static uintptr_t pm_test_end(const pmalloc_item_t *node) { return (uintptr_t)node + PM_TEST_HDR + pm_test_align_up(node->size); }

static uint32_t pm_test_check_chain(const pmalloc_item_t *list)
{
	uint32_t count = 0;
	const pmalloc_item_t *prev = NULL;
	for (const pmalloc_item_t *node = list; node != NULL; node = node->next) {
		TEST_ASSERT_TRUE_MESSAGE(node->prev == prev, "chain back link does not match forward link");
		TEST_ASSERT_TRUE_MESSAGE(prev == NULL || (uintptr_t)node > (uintptr_t)prev, "chain is not ascending by address");
		TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, (uint32_t)((uintptr_t)node % PM_TEST_ALIGN), "node is not aligned to PMALLOC_ALIGN");
		TEST_ASSERT_TRUE_MESSAGE(count < PM_TEST_MAX_NODES, "chain is too long or cyclic");
		prev = node;
		count++;
	}
	return count;
}

// Collects every node from both chains in address order. Returns the count.
static uint32_t pm_test_collect(const pmalloc_t *pm)
{
	const pmalloc_item_t *a = pm->available;
	const pmalloc_item_t *u = pm->assigned;
	uint32_t count = 0;
	while (a != NULL || u != NULL) {
		int take_free = (u == NULL) || (a != NULL && (uintptr_t)a < (uintptr_t)u);
		pm_test_nodes[count] = take_free ? a : u;
		pm_test_node_free[count] = take_free;
		if (take_free) {
			a = a->next;
		} else {
			u = u->next;
		}
		count++;
	}
	return count;
}

void pm_test_check_heap(pmalloc_t *pm, uint32_t regions)
{
	uint32_t free_nodes = pm_test_check_chain(pm->available);
	uint32_t used_nodes = pm_test_check_chain(pm->assigned);
	TEST_ASSERT_EQUAL_UINT32_MESSAGE(free_nodes + used_nodes, pm->totalnodes, "totalnodes does not count every node");

	uint32_t count = pm_test_collect(pm);
	uint64_t free_bytes = 0;
	uint64_t span_bytes = 0;
	for (uint32_t i = 0; i < count; i++) {
		const pmalloc_item_t *node = pm_test_nodes[i];
		if (pm_test_node_free[i]) {
			TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, node->size % PM_TEST_ALIGN, "free node size is not a multiple of PMALLOC_ALIGN");
			free_bytes += node->size;
		}
		span_bytes += pm_test_align_up(node->size);
		if (i > 0) {
			uintptr_t prev_end = pm_test_end(pm_test_nodes[i - 1]);
			TEST_ASSERT_TRUE_MESSAGE((uintptr_t)node >= prev_end, "nodes overlap");
			if (pm_test_node_free[i] && pm_test_node_free[i - 1]) {
				TEST_ASSERT_TRUE_MESSAGE((uintptr_t)node != prev_end, "adjacent free nodes were not merged");
			}
		}
	}

	TEST_ASSERT_EQUAL_UINT64_MESSAGE(free_bytes, (uint64_t)pmalloc_freemem(pm), "freemem is not the sum of free node sizes");
	TEST_ASSERT_TRUE_MESSAGE(pmalloc_freemem(pm) <= pmalloc_totalmem(pm), "freemem exceeds totalmem");
	TEST_ASSERT_EQUAL_UINT32_MESSAGE(pmalloc_totalmem(pm) - pmalloc_freemem(pm), pmalloc_usedmem(pm), "usedmem is not totalmem - freemem");
	TEST_ASSERT_EQUAL_UINT32_MESSAGE(pm->totalnodes * PM_TEST_HDR, pmalloc_overheadmem(pm), "overheadmem is not one header per node");
	TEST_ASSERT_TRUE_MESSAGE(pm->totalnodes >= regions, "fewer nodes than regions");
	TEST_ASSERT_EQUAL_UINT64_MESSAGE(span_bytes + (uint64_t)(pm->totalnodes - regions) * PM_TEST_HDR, (uint64_t)pmalloc_totalmem(pm),
	                                 "totalmem does not account for every node and split header");
}

void pm_test_check_tiling(pmalloc_t *pm, const void *start, const void *end)
{
	pm_test_check_heap(pm, 1);
	uint32_t count = pm_test_collect(pm);
	uintptr_t at = (uintptr_t)start;
	for (uint32_t i = 0; i < count; i++) {
		TEST_ASSERT_TRUE_MESSAGE((uintptr_t)pm_test_nodes[i] == at, "region has a gap or overlap between nodes");
		at = pm_test_end(pm_test_nodes[i]);
	}
	TEST_ASSERT_TRUE_MESSAGE(at == (uintptr_t)end, "nodes do not reach the end of the region");
}

void pm_test_check_poison(const unsigned char *from, uint32_t bytes)
{
	for (uint32_t i = 0; i < bytes; i++) {
		TEST_ASSERT_EQUAL_HEX8_MESSAGE(PM_TEST_POISON, from[i], "memory outside the region was written");
	}
}

static uint8_t pm_test_byte(uint32_t i, uint32_t seed) { return (uint8_t)((seed * 131u + i * 7u + 13u) & 0xFFu); }

void pm_test_fill(void *ptr, uint32_t bytes, uint32_t seed)
{
	unsigned char *p = (unsigned char *)ptr;
	for (uint32_t i = 0; i < bytes; i++) p[i] = pm_test_byte(i, seed);
}

int pm_test_verify(const void *ptr, uint32_t bytes, uint32_t seed)
{
	const unsigned char *p = (const unsigned char *)ptr;
	for (uint32_t i = 0; i < bytes; i++) {
		if (p[i] != pm_test_byte(i, seed)) return 0;
	}
	return 1;
}
