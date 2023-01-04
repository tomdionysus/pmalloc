#ifndef PMALLOC
#define PMALLOC

#include <stdint.h>
#include <stdlib.h>

void pmalloc_addblock(void *ptr, uint32_t size);	// Add an area of memory available for allocation
void *pmalloc_malloc(uint32_t size);				// Allocate size bytes of memory, returns NULL if out of memory
void *pmalloc_calloc(uint32_t num, uint32_t size);	// Allocate num blocks each of size bytes, clear the memory first
void pmalloc_free(void *ptr);						// Deallocate a block of previously allocated memory

uint32_t pmalloc_sizeof(void *ptr);					// Return the size of a block of previously allocated memory
uint32_t pmalloc_freemem();							// Return the amount of free memory	
uint32_t pmalloc_totalmem();						// Return the total amount of memory
uint32_t pmalloc_usedmem();							// Return the amount of used memory
uint32_t pmalloc_overheadmem();						// Return the current memory overhead

#endif