//
// pmalloc - A Heap based memory manager
//
// Author: Tom Cully <mail@tomcully.com>
// Date: 10th Dec 2022
//
// pmalloc can be used to manage memory using a familiar malloc/free pattern in embedded 
// systems, or to manage sub-allocation within an area of memory given by the OS.
//
// To get started, call paddblock with the address and size of the memory to be used for 
// allocation.
//

#include "pmalloc.h"

// Local linked list definitions
typedef struct ll_item_t {
	struct ll_item_t *prev;								// The previous block in the chain
	struct ll_item_t *next;								// The next block in the chain
	uint32_t size;										// This is the size of the block as reported to the user 
} ll_item_t;

void ll_remove(ll_item_t **root, ll_item_t *node);
void ll_insert_sorted(ll_item_t **root, void *ptr);

// Global available/assigned list and metrics
ll_item_t *__available = NULL;
ll_item_t *__assigned = NULL;
uint32_t __freemem = 0;
uint32_t __totalmem = 0;
uint32_t __totalnodes = 0;

void pmalloc_addblock(void *ptr, uint32_t size)
{
	// Get the usable size of the block
	((ll_item_t*)ptr)->size = size - sizeof(ll_item_t);

	// Update freemem and totalmem
	__freemem += ((ll_item_t*)ptr)->size;
	__totalmem += __freemem;

	// Add it to the available heap, update totalnodes
	ll_insert_sorted(&__available, ptr);
	__totalnodes++;
}

void *pmalloc_malloc(uint32_t size)
{
	// Find a suitable block
	ll_item_t *current = __available;
	while(current != NULL && current->size < size) current = current->next;

	// If there's nothing suitable, we're either out of memory or fragged.
	if(current == NULL) return NULL;

	// Remove it from __available
	ll_remove(&__available, current);

	// Resize and add to __assigned
	ll_insert_sorted(&__assigned, current);

	// If it's not the exact size..
	if(current->size != size) {
		// Add a free block that's the remainder size
		ll_item_t *newfree = current + sizeof(ll_item_t) + size;
		newfree->size = current->size - sizeof(ll_item_t) - size;
		// Change __assigned size
		current->size = size;
		ll_insert_sorted(&__available, newfree);

		// We've lost a bit of overhead making the new node
		__freemem -= sizeof(ll_item_t);
		__totalnodes++;
	}

	// Reduce the amount of free memory
	__freemem -= current->size;

	// Return the user memory
	return current + sizeof(ll_item_t);
}

void *pmalloc_calloc(uint32_t num, uint32_t size)
{
	char *mem = pmalloc_malloc(num * size);
	if(mem==NULL) return NULL;
	for(uint32_t i=0; i<size; i++) mem[i]=0;
	return mem;
}

void pmalloc_free(void *ptr)
{
	// Get the node of this memory
	ll_item_t *node = (ll_item_t*)ptr - sizeof(ll_item_t);

	// Remove it from __assigned
	ll_remove(&__assigned, node);

	__freemem += node->size;

	// Add to __available
	ll_insert_sorted(&__available, node);

	// Scan backward for contiguious blocks
	while(node->prev!=NULL && node == node->prev + sizeof(ll_item_t) + node->prev->size) node = node->prev;

	// Scan forward and merge free blocks
	while(node->next == node + sizeof(ll_item_t) + node->size) {
		uint32_t nodesize = node->next->size + sizeof(ll_item_t);
		__freemem += sizeof(ll_item_t);
		ll_remove(&__available, node->next);
		__totalnodes--;
		node->size += nodesize;
	}
}

uint32_t pmalloc_sizeof(void *ptr) {
	// Get the actual ll_item_t of the block
	ll_item_t *node = (ll_item_t*)ptr - sizeof(ll_item_t);

	// Return its size
	return node->size;
}

void ll_remove(ll_item_t **root, ll_item_t *node) 
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

void ll_insert_sorted(ll_item_t **root, void *ptr)
{
	// No existing root
	if(*root == NULL) {
		*root = (ll_item_t*)ptr;
		(*root)->prev = NULL;
		(*root)->next = NULL;
		return;
	}

	// Where is the block in relation to the root?
	if(ptr < (void*)*root) {
		// New block before root
		ll_item_t *node = (ll_item_t*)ptr;
		ll_item_t *oldroot = *root;
		oldroot->prev = node;
		node->next = oldroot;
		*root = node;
	} else {
		// New block within or at end of list
		ll_item_t *current = *root;
		ll_item_t *node = (ll_item_t*)ptr;
		
		// Skip until we find the right place to insert, or the end of the list
		while(current->next != NULL && node > current->next) current = current->next;

		// We're inserting the block at...
		if(current->next == NULL) {
			// The end of List
			node->prev = current;
			current->next = node;
		} else {
			// Somewhere in the middle
			ll_item_t *oldnext = current->next;

			current->next = node;
			node->prev = current;
			node->next = oldnext;
			oldnext->prev = node;
		}
	} 
}

uint32_t pmalloc_totalmem() { return __totalmem; }
uint32_t pmalloc_freemem() { return __freemem; }
uint32_t pmalloc_usedmem() { return __totalmem - __freemem; }
uint32_t pmalloc_overheadmem() { return __totalnodes * sizeof(ll_item_t); }