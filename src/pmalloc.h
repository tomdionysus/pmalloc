#ifndef PMALLOC
#define PMALLOC

#include <stdint.h>
#include <stddef.h>

typedef struct pmalloc_item {
    struct pmalloc_item *prev;  // The previous block in the chain
    struct pmalloc_item *next;  // The next block in the chain
    uint32_t size;              // This is the size of the block as reported to the user 
} pmalloc_item_t;

typedef struct pmalloc {
    pmalloc_item_t *available;  // The linked list of available blocks
    pmalloc_item_t *assigned;   // The linked list of allocated blocks
    uint32_t freemem;           // The current free memory count
    uint32_t totalmem;          // The total available free memory
    uint32_t totalnodes;        // The number of nodes in the allocated list
} pmalloc_t;

void pmalloc_init(pmalloc_t *pm);
void pmalloc_addblock(pmalloc_t *pm, void *ptr, uint32_t size);         // Add an area of memory available for allocation
void *pmalloc_malloc(pmalloc_t *pm, uint32_t size);                     // Allocate size bytes of memory, returns NULL if out of memory
void *pmalloc_calloc(pmalloc_t *pm, uint32_t num, uint32_t size);       // Allocate num blocks each of size bytes, clear the memory first
void *pmalloc_realloc(pmalloc_t *pm, void *ptr, uint32_t size);         // Reallocate the existing block ptr to a new size and return the new block
void pmalloc_free(pmalloc_t *pm, void *ptr);                            // Deallocate a block of previously allocated memory

uint32_t pmalloc_sizeof(pmalloc_t *pm, void *ptr);                      // Return the size of a block of previously allocated memory
uint32_t pmalloc_freemem(pmalloc_t *pm);                                // Return the amount of free memory 
uint32_t pmalloc_totalmem(pmalloc_t *pm);                               // Return the total amount of memory
uint32_t pmalloc_usedmem(pmalloc_t *pm);                                // Return the amount of used memory
uint32_t pmalloc_overheadmem(pmalloc_t *pm);                            // Return the current memory overhead

// Internals
void pmalloc_merge(pmalloc_t *pm, pmalloc_item_t* node);                // Merge free blocks around this block
void pmalloc_item_insert(pmalloc_item_t **root, void *ptr);             // Insert an item into the linked list
void pmalloc_item_remove(pmalloc_item_t **root, pmalloc_item_t *node);  // Remove an item from a linked list

#ifdef DEBUG
void pmalloc_dump_stats(pmalloc_t *pm);                                 // Debug Function
#endif

#endif

