#ifndef PMALLOC
#define PMALLOC

#include <stddef.h>
#include <stdint.h>

#define PMALLOC_VERSION_MAJOR 0
#define PMALLOC_VERSION_MINOR 2
#define PMALLOC_VERSION_PATCH 0
#define PMALLOC_VERSION "0.2.0"

// Alignment, in bytes, of every block header and every pointer pmalloc returns. Must be a power
// of two no smaller than the alignment of a pointer. Override at compile time, for example
// -DPMALLOC_ALIGN=16, and build pmalloc.c and its callers with the same value.
#ifndef PMALLOC_ALIGN
#define PMALLOC_ALIGN sizeof(void*)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pmalloc_item {
    struct pmalloc_item* prev;  // The previous block in the chain
    struct pmalloc_item* next;  // The next block in the chain
    uint32_t size;              // This is the size of the block as reported to the user
} pmalloc_item_t;

typedef struct pmalloc {
    pmalloc_item_t* available;  // The linked list of available blocks
    pmalloc_item_t* assigned;   // The linked list of allocated blocks
    uint32_t freemem;           // The current free memory count
    uint32_t totalmem;          // The total available free memory
    uint32_t totalnodes;        // The number of nodes in the allocated list
} pmalloc_t;

void pmalloc_init(pmalloc_t* pm);

// Add an area of memory available for allocation
void pmalloc_addblock(pmalloc_t* pm, void* ptr, uint32_t size);

// Allocate size bytes of memory, returns NULL if out of memory
void* pmalloc_malloc(pmalloc_t* pm, uint32_t size);

// Allocate num blocks each of size bytes, clear the memory first
void* pmalloc_calloc(pmalloc_t* pm, uint32_t num, uint32_t size);

// Reallocate the existing block ptr to a new size and return the new block
void* pmalloc_realloc(pmalloc_t* pm, void* ptr, uint32_t size);

// Deallocate a block of previously allocated memory
void pmalloc_free(pmalloc_t* pm, void* ptr);

// Return the size of a block of previously allocated memory
uint32_t pmalloc_sizeof(pmalloc_t* pm, void* ptr);

uint32_t pmalloc_freemem(pmalloc_t* pm);      // Return the amount of free memory
uint32_t pmalloc_totalmem(pmalloc_t* pm);     // Return the total amount of memory
uint32_t pmalloc_usedmem(pmalloc_t* pm);      // Return the amount of used memory
uint32_t pmalloc_overheadmem(pmalloc_t* pm);  // Return the current memory overhead

// Internals

// Merge free blocks around this block
void pmalloc_merge(pmalloc_t* pm, pmalloc_item_t* node);

// Insert an item into the linked list
void pmalloc_item_insert(pmalloc_item_t** root, void* ptr);

// Remove an item from a linked list
void pmalloc_item_remove(pmalloc_item_t** root, pmalloc_item_t* node);

#ifdef DEBUG
// Debug Function
void pmalloc_dump_stats(pmalloc_t* pm);
#endif

#ifdef __cplusplus
}
#endif

#endif
