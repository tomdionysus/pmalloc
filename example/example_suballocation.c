#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pmalloc.h"

int main() {
	printf("pmalloc: Suballocation Example\n\n");

	const uint32_t MEMORY_SIZE = 1024*1024;

	// Set up a megabyte of memory with the OS malloc
	char *memory = malloc(MEMORY_SIZE);
	if(memory == NULL) return 1;

	// Our pmalloc struct
	pmalloc_t pmm; 
	pmalloc_t *pm = &pmm;

	pmalloc_init(pm);
	pmalloc_addblock(pm, memory, MEMORY_SIZE);

	printf("Allocating\n");

	char *strs[] = {
		"One",
		"Two",
		"Three",
		"Four",
		"Five",
		"Six",
		"Seven",
		"Eight",
		"Nine",
		"Ten"
	};

	char *ptrs[10];

	for(uint8_t i=0; i<10; i++) {
		uint8_t len = strlen(strs[i])+1;
		printf("Allocating %d bytes for '%s', ", len, strs[i]);
		ptrs[i] = (char*)pmalloc_malloc(pm, len); 
		printf("Copying, ");
		strcpy(ptrs[i], strs[i]);
		printf("Done.\n");
	}

	printf("Printing from Allocations\n");

	for(uint8_t i=0; i<10; i++) {
		printf("'%s' is length %d\n", ptrs[i], pmalloc_sizeof(pm, ptrs[i]));
	}

	printf("Removing 4 and 5\n");

	pmalloc_free(pm, ptrs[3]);
	pmalloc_free(pm, ptrs[4]);
	ptrs[3] = NULL;
	ptrs[4] = NULL;

	printf("Reallocating 'Eleven'\n");

	char *eleven = "Eleven";
	uint8_t len = strlen(eleven);
	printf("Allocating %d bytes for '%s', ", len, eleven);
	ptrs[3] = (char*)pmalloc_malloc(pm, len); 
	printf("Copying, ");
	strcpy(ptrs[3], eleven);
	printf("Done.\n");

	printf("Printing from Allocations\n");

	for(uint8_t i=0; i<10; i++) {
		if(ptrs[i]==NULL) continue;
		printf("'%s' is length %d\n", ptrs[i], pmalloc_sizeof(pm, ptrs[i]));
	}

	// Note we don't have to pmalloc_free our allocations here - we're just dropping the entire memory back to the OS.

	// Free the OS Memory
	free(memory);

	return 0;
}
