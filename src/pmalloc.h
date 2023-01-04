#include <stdint.h>

#ifndef PMALLOC
#define PMALLOC

void paddblock(void *ptr, uint32_t size);				// Add an area of memory available for allocation
void *pmalloc(uint32_t size);							// Allocate size bytes of memory, returns NULL if out of memory
void *pcalloc(uint32_t num, uint32_t size);				// Allocate num blocks each of size bytes, clear the memory first
void pfree(void *ptr);									// Deallocate a block of previously allocated memory

uint32_t psizeof(void *ptr);							// Return the size of a block of previously allocated memory

uint32_t pfreemem();									// Return the amount of free memory	
uint32_t ptotalmem();									// Return the total amount of memory	
void pmalloc_dump();									// Dump all available and allocated blocks to console

#endif