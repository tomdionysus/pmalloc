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

#include "pmalloc.h"

#ifdef DEBUG
	#include <stdio.h>
#endif

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
	// Get the usable size of the block
	((pmalloc_item_t*)ptr)->size = size - sizeof(pmalloc_item_t);

	// Update freemem and totalmem
	pm->freemem += ((pmalloc_item_t*)ptr)->size;
	pm->totalmem += pm->freemem;

	// Add it to the available heap, update totalnodes
	pmalloc_item_insert(&pm->available, ptr);
	pm->totalnodes++;
}

void *pmalloc_malloc(pmalloc_t *pm, uint32_t size)
{
	// Find a suitable block
	pmalloc_item_t *current = pm->available;
	while(current != NULL && current->size < size) current = current->next;

	// If there's nothing suitable, we're either out of memory or fragged.
	if(current == NULL) return NULL;

	// Remove it from pm->available
	pmalloc_item_remove(&pm->available, current);

	// Add to pm->assigned
	pmalloc_item_insert(&pm->assigned, current);

	// If it's not the exact size..
	if(current->size != size) {
		// Add a free block that's the remainder size
		pmalloc_item_t *newfree = (pmalloc_item_t*)((char*)current + sizeof(pmalloc_item_t) + size);
		newfree->size = current->size - sizeof(pmalloc_item_t) - size;
		newfree->next = current->next;
		current->next = NULL;
		
		// Change pm->assigned size
		current->size = size;
		pmalloc_item_insert(&pm->available, newfree);

		// We've lost a bit of overhead making the new node
		pm->freemem -= sizeof(pmalloc_item_t);
		pm->totalnodes++;

		// Merge around newfree
		pmalloc_merge(pm, newfree);
	}

	// Reduce the amount of free memory
	pm->freemem -= current->size;

	// Return the user memory
	return current + sizeof(pmalloc_item_t);
}

void *pmalloc_calloc(pmalloc_t *pm, uint32_t num, uint32_t size)
{
	char *mem = pmalloc_malloc(pm, num * size);
	if(mem==NULL) return NULL;
	for(uint32_t i=0; i<size; i++) mem[i]=0;
	return mem;
}

void pmalloc_free(pmalloc_t *pm, void *ptr)
{
	// Get the node of this memory
	pmalloc_item_t *node = (pmalloc_item_t*)ptr - sizeof(pmalloc_item_t);

	// Remove it from pm->assigned
	pmalloc_item_remove(&pm->assigned, node);

	pm->freemem += node->size;

	// Add to pm->available
	pmalloc_item_insert(&pm->available, node);

	// Merge around current
	pmalloc_merge(pm, node);
}

void pmalloc_merge(pmalloc_t *pm, pmalloc_item_t* node) {
	// Scan backward for contiguous blocks
	while (node->prev != NULL && (char*)node == (char*)node->prev + sizeof(pmalloc_item_t) + node->prev->size)
		node = node->prev;

	// Scan forward and merge free blocks
	while (node->next == (pmalloc_item_t*)((char*)node + sizeof(pmalloc_item_t) + node->size)) {
		uint32_t nodesize = node->next->size + sizeof(pmalloc_item_t);
		pm->freemem += sizeof(pmalloc_item_t);
		pmalloc_item_remove(&pm->available, node->next);
		pm->totalnodes--;
		node->size += nodesize;
	}
}

uint32_t pmalloc_sizeof(pmalloc_t *pm, void *ptr) {
	// Get the actual pmalloc_item_t of the block
	pmalloc_item_t *node = (pmalloc_item_t*)ptr - sizeof(pmalloc_item_t);

	// Return its size
	return node->size;
}

uint32_t pmalloc_totalmem(pmalloc_t *pm) { return pm->totalmem; }
uint32_t pmalloc_freemem(pmalloc_t *pm) { return pm->freemem; }
uint32_t pmalloc_usedmem(pmalloc_t *pm) { return pm->totalmem - pm->freemem; }
uint32_t pmalloc_overheadmem(pmalloc_t *pm) { return pm->totalnodes * sizeof(pmalloc_item_t); }

void pmalloc_item_insert(pmalloc_item_t **root, void *ptr)
{
	// No existing root
	if(*root == NULL) {
		*root = (pmalloc_item_t*)ptr;
		(*root)->prev = NULL;
		(*root)->next = NULL;
		return;
	}

	// Where is the block in relation to the root?
	if(ptr < (void*)*root) {
		// New block before root
		pmalloc_item_t *node = (pmalloc_item_t*)ptr;
		pmalloc_item_t *oldroot = *root;
		oldroot->prev = node;
		node->next = oldroot;
		*root = node;
	} else {
		// New block within or at end of list
		pmalloc_item_t *current = *root;
		pmalloc_item_t *node = (pmalloc_item_t*)ptr;
		
		// Skip until we find the right place to insert, or the end of the list
		while(current->next != NULL && node > current->next) current = current->next;

		// We're inserting the block at...
		if(current->next == NULL) {
			// The end of list
			node->prev = current;
			current->next = node;
		} else {
			// Somewhere in the middle
			pmalloc_item_t *oldnext = current->next;

			current->next = node;
			node->prev = current;
			node->next = oldnext;
			oldnext->prev = node;
		}
	} 
}

void pmalloc_item_remove(pmalloc_item_t **root, pmalloc_item_t *node) 
{
	// Remove the node
	if(node->prev) node->prev->next = node->next;
	if(node->next) node->next->prev = node->prev;
	
	// Fixup root if the node was root
	if(node==*root) {
		if (node->prev) 
			*root = node->prev; 
		else 
			*root = node->next; 
	}

	// Clear the next and previous pointers
	node->next = NULL;
	node->prev = NULL;
}

#ifdef DEBUG
void pmalloc_dump_stats(pmalloc_t *pm) {
	printf("---------------------\n");
	printf(" - freemem: %d\n", pm->freemem);
	printf(" - totalmem: %d\n", pm->totalmem);
	printf(" - totalnodes: %d (sizeof %d)\n", pm->totalnodes, (int)sizeof(pmalloc_item_t));
	printf(" - assigned:\n");
	for(pmalloc_item_t* current = pm->assigned; current != NULL; current=current->next) {
		printf("  - size: %d\n", current->size);
	} 
	printf(" - available:\n");
	for(pmalloc_item_t* current = pm->available; current != NULL; current=current->next) {
		printf("  - size: %d\n", current->size);
	} 

	printf("---------------------\n");
}
#endif