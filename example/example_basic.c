#include <stdio.h>
#include "pmalloc.h"

int main(void) {
	printf("pmalloc: Basic Example\n\n");

	pmalloc_t pmblock;
	pmalloc_t *pm = &pmblock;

	// Initialise our pmalloc
	pmalloc_init(pm);

	// Make 64k of memory - in embedded systems, we'd actually give a static memory address and size.
	static char buffer[65536];
	pmalloc_addblock(pm, buffer, sizeof(buffer));

	uint32_t len[6] = { 150, 256, 512, 100, 1024, 65536 };
	void* mem[6];

	// Allocate some memory - the last allocation should fail
	for(uint32_t i = 0; i<6; i++) {
		printf("Allocating %lu bytes...\n", (unsigned long)len[i]);
		mem[i] = pmalloc_malloc(pm, len[i]);
	}
	if(mem[5] == NULL) printf("Last allocation of %lu bytes failed as expected\n", (unsigned long)len[5]);

	// ...use the memory...

	printf("Deallocating\n");
	// Deallocate the memory
	for(uint32_t i = 0; i<5; i++) pmalloc_free(pm, mem[i]);

	printf("Done\n");
	return 0;
}
