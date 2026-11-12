//
// pmalloc - A Heap based memory manager
//
// Author: Tom Cully <mail@tomcully.com>
// Date: 10th Dec 2022
//
// pmalloc can be used to manage memory using a familiar malloc/free pattern in embedded
// systems, or to manage sub-allocation within an area of memory given by the OS.
//
// To get started, call pmalloc_addblock with the address and size of the memory to be used for
// allocation.
//
// Layout: every block is a header followed by its payload. The header is a pmalloc_item_t padded
// to PMALLOC_ALIGN bytes, and every payload occupies its size rounded up to PMALLOC_ALIGN (its
// span), so every header and every payload is aligned. Free blocks always hold a size that is a
// multiple of PMALLOC_ALIGN. An assigned block holds the size the caller asked for, or, when the
// block took a tail too small to become a free block of its own, its whole span.
//
// Both chains are kept in address order. No two free blocks are ever adjacent: each one is merged
// with its neighbours when it is created. Regions added with pmalloc_addblock are kept apart so
// they are never merged, which keeps totalmem exact.
//

#include "pmalloc.h"

#ifdef DEBUG
	#include <stdio.h>
#endif

// PMALLOC_ALIGN must be a power of two that can hold a pointer.
typedef char pmalloc_align_is_a_power_of_two[((PMALLOC_ALIGN) & ((PMALLOC_ALIGN) - 1)) == 0 ? 1 : -1];
typedef char pmalloc_align_holds_a_pointer[(PMALLOC_ALIGN) >= sizeof(void *) ? 1 : -1];

#define PM_ALIGN ((uint32_t)(PMALLOC_ALIGN))
#define PM_MASK (PM_ALIGN - 1u)
#define PM_HDR ((((uint32_t)sizeof(pmalloc_item_t)) + PM_MASK) & ~PM_MASK)

// The smallest tail worth returning to the free chain: a header and one alignment unit.
#define PM_MIN_SPLIT (PM_HDR + PM_ALIGN)

static uint32_t pm_span(const pmalloc_item_t *node) { return (node->size + PM_MASK) & ~PM_MASK; }
static uintptr_t pm_end(const pmalloc_item_t *node) { return (uintptr_t)node + PM_HDR + pm_span(node); }
static pmalloc_item_t *pm_node(void *ptr) { return (pmalloc_item_t *)((uintptr_t)ptr - PM_HDR); }
static void *pm_payload(const pmalloc_item_t *node) { return (void *)((uintptr_t)node + PM_HDR); }

// Rounds size up to PMALLOC_ALIGN. Returns 0 if the result would not fit in 32 bits.
static int pm_round(uint32_t size, uint32_t *rounded)
{
	if (size > UINT32_MAX - PM_MASK) return 0;
	*rounded = (size + PM_MASK) & ~PM_MASK;
	return 1;
}

// Cuts an assigned block whose span is `have` bytes down to `want` bytes, returning the tail to the
// free chain when it is big enough to be a block of its own. Otherwise the block keeps the tail.
static void pm_trim(pmalloc_t *pm, pmalloc_item_t *node, uint32_t have, uint32_t want, uint32_t size)
{
	uint32_t tail = have - want;
	if (tail < PM_MIN_SPLIT) {
		node->size = tail == 0 ? size : have;
		return;
	}

	pmalloc_item_t *rest = (pmalloc_item_t *)((uintptr_t)node + PM_HDR + want);
	rest->size = tail - PM_HDR;
	node->size = size;
	pmalloc_item_insert(&pm->available, rest);
	pm->freemem += rest->size;
	pm->totalnodes++;
	pmalloc_merge(pm, rest);
}

void pmalloc_init(pmalloc_t *pm) {
	#ifdef DEBUG
		printf("pmalloc: DEBUG Enabled\n");
	#endif

	pm->available = NULL;
	pm->assigned = NULL;
	pm->freemem = 0;
	pm->totalmem = 0;
	pm->totalnodes = 0;
}

void pmalloc_addblock(pmalloc_t *pm, void *ptr, uint32_t size)
{
	if (ptr == NULL) return;

	// Use only the aligned part of the region
	uintptr_t start = ((uintptr_t)ptr + PM_MASK) & ~(uintptr_t)PM_MASK;
	uintptr_t end = ((uintptr_t)ptr + size) & ~(uintptr_t)PM_MASK;

	// Keep one alignment unit between this region and any block it touches, so the two are never merged
	pmalloc_item_t *chains[2] = { pm->available, pm->assigned };
	for (int i = 0; i < 2; i++) {
		for (pmalloc_item_t *node = chains[i]; node != NULL; node = node->next) {
			if (pm_end(node) == start) start += PM_ALIGN;
			if ((uintptr_t)node == end) end -= PM_ALIGN;
		}
	}

	// Ignore a region too small for a header and one alignment unit
	if (end <= start + PM_HDR) return;

	pmalloc_item_t *node = (pmalloc_item_t *)start;
	node->size = (uint32_t)(end - start) - PM_HDR;
	pm->freemem += node->size;
	pm->totalmem += node->size;
	pm->totalnodes++;
	pmalloc_item_insert(&pm->available, node);
}

void *pmalloc_malloc(pmalloc_t *pm, uint32_t size)
{
	uint32_t want;
	if (!pm_round(size, &want)) return NULL;

	// First fit, in address order
	pmalloc_item_t *node = pm->available;
	while (node != NULL && node->size < want) node = node->next;
	if (node == NULL) return NULL;

	uint32_t have = node->size;
	pmalloc_item_remove(&pm->available, node);
	pmalloc_item_insert(&pm->assigned, node);
	pm->freemem -= have;
	pm_trim(pm, node, have, want, size);

	return pm_payload(node);
}

void *pmalloc_calloc(pmalloc_t *pm, uint32_t num, uint32_t size)
{
	if (size != 0 && num > UINT32_MAX / size) return NULL;
	uint32_t total = num * size;

	unsigned char *mem = (unsigned char *)pmalloc_malloc(pm, total);
	if (mem == NULL) return NULL;
	for (uint32_t i = 0; i < total; i++) mem[i] = 0;
	return mem;
}

void *pmalloc_realloc(pmalloc_t *pm, void *ptr, uint32_t size)
{
	// Match stdlib realloc() NULL interface
	if (ptr == NULL) return pmalloc_malloc(pm, size);

	uint32_t want;
	if (!pm_round(size, &want)) return NULL;

	pmalloc_item_t *node = pm_node(ptr);
	uint32_t have = pm_span(node);

	// Shrinking, or growing within the current span: stay in place
	if (want <= have) {
		pm_trim(pm, node, have, want, size);
		return ptr;
	}

	// Growing: stay in place if the next block is free and covers the difference
	uintptr_t end = pm_end(node);
	pmalloc_item_t *next = pm->available;
	while (next != NULL && (uintptr_t)next < end) next = next->next;
	if (next != NULL && (uintptr_t)next == end && have + PM_HDR + next->size >= want) {
		uint32_t total = have + PM_HDR + next->size;
		pmalloc_item_remove(&pm->available, next);
		pm->freemem -= next->size;
		pm->totalnodes--;
		pm_trim(pm, node, total, want, size);
		return ptr;
	}

	// Otherwise move: allocate, copy, free. On failure the original block is untouched.
	unsigned char *moved = (unsigned char *)pmalloc_malloc(pm, size);
	if (moved == NULL) return NULL;
	const unsigned char *from = (const unsigned char *)ptr;
	for (uint32_t i = 0; i < node->size; i++) moved[i] = from[i];
	pmalloc_free(pm, ptr);
	return moved;
}

void pmalloc_free(pmalloc_t *pm, void *ptr)
{
	// Match stdlib free() NULL interface
	if (ptr == NULL) return;

	pmalloc_item_t *node = pm_node(ptr);
	pmalloc_item_remove(&pm->assigned, node);
	node->size = pm_span(node);
	pm->freemem += node->size;
	pmalloc_item_insert(&pm->available, node);
	pmalloc_merge(pm, node);
}

void pmalloc_merge(pmalloc_t *pm, pmalloc_item_t *node)
{
	// Scan backward to the first of a run of adjacent free blocks
	while (node->prev != NULL && pm_end(node->prev) == (uintptr_t)node) node = node->prev;

	// Fold every adjacent free block after it into it
	while (node->next != NULL && pm_end(node) == (uintptr_t)node->next) {
		pmalloc_item_t *next = node->next;
		pmalloc_item_remove(&pm->available, next);
		node->size += PM_HDR + next->size;
		pm->freemem += PM_HDR;
		pm->totalnodes--;
	}
}

uint32_t pmalloc_sizeof(pmalloc_t *pm, void *ptr)
{
	(void)pm;
	if (ptr == NULL) return 0;
	return pm_node(ptr)->size;
}

uint32_t pmalloc_totalmem(pmalloc_t *pm) { return pm->totalmem; }
uint32_t pmalloc_freemem(pmalloc_t *pm) { return pm->freemem; }
uint32_t pmalloc_usedmem(pmalloc_t *pm) { return pm->totalmem - pm->freemem; }
uint32_t pmalloc_overheadmem(pmalloc_t *pm) { return pm->totalnodes * PM_HDR; }

void pmalloc_item_insert(pmalloc_item_t **root, void *ptr)
{
	pmalloc_item_t *node = (pmalloc_item_t *)ptr;
	pmalloc_item_t *prev = NULL;
	pmalloc_item_t *next = *root;

	// Find the place in address order
	while (next != NULL && (uintptr_t)next < (uintptr_t)node) {
		prev = next;
		next = next->next;
	}

	node->prev = prev;
	node->next = next;
	if (prev != NULL) prev->next = node; else *root = node;
	if (next != NULL) next->prev = node;
}

void pmalloc_item_remove(pmalloc_item_t **root, pmalloc_item_t *node)
{
	if (node->prev != NULL) node->prev->next = node->next; else *root = node->next;
	if (node->next != NULL) node->next->prev = node->prev;

	node->next = NULL;
	node->prev = NULL;
}

#ifdef DEBUG
static void pm_dump_chain(const pmalloc_item_t *node)
{
	for (; node != NULL; node = node->next) {
		printf("  - (%p) %p -> %p - size: %lu (%lu sys, %lu usr)\n", (const void *)node, pm_payload(node), (void *)pm_end(node),
		       (unsigned long)(PM_HDR + pm_span(node)), (unsigned long)PM_HDR, (unsigned long)node->size);
	}
}

void pmalloc_dump_stats(pmalloc_t *pm) {
	printf("---------------------\n");
	printf(" - freemem: %lu\n", (unsigned long)pm->freemem);
	printf(" - totalmem: %lu\n", (unsigned long)pm->totalmem);
	printf(" - totalnodes: %lu (header %lu, align %lu)\n", (unsigned long)pm->totalnodes, (unsigned long)PM_HDR, (unsigned long)PM_ALIGN);
	printf(" - assigned:\n");
	pm_dump_chain(pm->assigned);
	printf(" - available:\n");
	pm_dump_chain(pm->available);
	printf("---------------------\n");
}
#endif
