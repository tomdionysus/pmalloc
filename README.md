# pmalloc

[![CircleCI](https://dl.circleci.com/status-badge/img/gh/tomdionysus/pmalloc/tree/master.svg?style=svg)](https://dl.circleci.com/status-badge/redirect/gh/tomdionysus/pmalloc/tree/master) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT) 

A extremely minimal C library (5 KB) to manage memory using a familiar malloc/free pattern in embedded systems, or to manage sub-allocation within an area of memory given by the OS.

**pmalloc is BETA: Use at your own risk.**

## Building

pmalloc uses [CMake](https://cmake.org/) so building is trivial:

```bash
mkdir build
cd build
cmake ..
make
```

You'll find `libpmalloc.a` in the `build/lib` folder. 

## Testing

pmalloc uses [GoogleTest](https://github.com/google/googletest) which is installed by **CMake**:

```bash
mkdir build
cd build
cmake ..
make
./pmalloc_test
```

## Debug Build / Test

pmalloc will dump extra debug information in testing (and enable the `pmalloc_dump_stats` function) if built in debug mode:

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug .. 
make
./pmalloc_test
```

## Getting Started

A simple example of use:

```C
#include <stdio.h>
#include "pmalloc.h"

int main() {
  printf("pmalloc: Basic Example\n\n");

  pmalloc_t pmblock;
  pmalloc_t *pm = &pmblock;

  // Initialise our pmalloc
  pmalloc_init(pm);

  // Make 64k of memory - in embedded systems, we'd actually give a static memory address and size.
  char buffer[65536];
  pmalloc_addblock(pm, &buffer, 65536);

  uint32_t len[6] = { 150, 256, 512, 100, 1024, 65536 };
  void* mem[6];

  // Allocate some memory - the last allocation should fail
  for(uint32_t i = 0; i<6; i++) {
    printf("Allocating %d bytes...\n", len[i]);
    mem[i] = pmalloc_malloc(pm, len[i]);
  }
  if(mem[5] == NULL) printf("Last allocation of %d bytes failed as expected\n", len[5]); 

  // ...use the memory...

  printf("Deallocaing\n");
  // Deallocate the memory
  for(uint32_t i = 0; i<5; i++) pmalloc_free(pm, mem[i]);
  
  printf("Done\n");
}
```

## Examples

There are a number of further examples in the [example](example) directory.

* [Basic Example](example/example_basic.c) 
* [Suballocation Example](example/example_suballocation.c) 

## Structures

### pmalloc_t

```C
typedef struct pmalloc {
  pmalloc_ll_item_t *available;
  pmalloc_ll_item_t *assigned;
  uint32_t freemem;
  uint32_t totalmem;
  uint32_t totalnodes;
} pmalloc_t;
```

This represents the root of the allocation structure.

### pmalloc_item_t

```C
typedef struct pmalloc_item {
  struct pmalloc_item *prev;                  // The previous block in the chain
  struct pmalloc_item *next;                  // The next block in the chain
  uint32_t size;                          // This is the size of the block as reported to the user 
} pmalloc_item_t;
```

A linked-list item representing an individual memory allocation.

## Functions

### pmalloc_init

`void pmalloc_init(pmalloc_t *pm)`

Initialise the specified `pmalloc_t` structure.

### pmalloc_addblock

`void pmalloc_addblock(pmalloc_t *pm, void *ptr, uint32_t size)`

Add memory at `ptr` of byte size `size` to be available for allocation using `pmalloc_malloc` or `pmalloc_calloc`.

### pmalloc_malloc

`void *pmalloc_malloc(pmalloc_t *pm, uint32_t size)`

Allocate a block of memory of `size` bytes from the available space. Return a pointer to the block, or `NULL` if there isn't enough space.

### pmalloc_calloc

`void *pmalloc_calloc(pmalloc_t *pm, uint32_t num, uint32_t size)`

Allocate `num` blocks of memory of `size` bytes from the available space and fill it with `0x00`. Return a pointer to the first block, or `NULL` if there isn't enough space.

### pmalloc_realloc

`void *pmalloc_realloc(pmalloc_t *pm, void *ptr, uint32_t size)`

Reallocate the block of previously allocated memory pointed to by `ptr` to a new size and return the new block pointer.
Note: If the block cannot be reallocated, `pmalloc_realloc` will return NULL without freeing the existing block.
Note: If the block must be relocated, `pmalloc_realloc` will copy the existing memory in the block and return the new pointer - this may be expensive depending on the length of the block.

### pmalloc_free

`void pmalloc_free(pmalloc_t *pm, void *ptr)`

Free the block of previously allocated memory pointed to by `ptr`.

### pmalloc_sizeof

`uint32_t pmalloc_sizeof(pmalloc_t *pm, void *ptr)`

Get the size in bytes of the block previously allocated memory pointed to by `ptr`.

### pmalloc_freemem

`uint32_t pmalloc_freemem(pmalloc_t *pm)`

Return the currently available free memory in bytes.

### pmalloc_totalmem

`uint32_t pmalloc_totalmem(pmalloc_t *pm)`

Return the total memory in bytes.

### pmalloc_usedmem

`uint32_t pmalloc_usedmem(pmalloc_t *pm)`

Return the amount of used memory in bytes.

### pmalloc_overheadmem

`uint32_t pmalloc_overheadmem(pmalloc_t *pm)`

Return the current amount of memory consumed in overhead in bytes.

### pmalloc_merge

`void pmalloc_merge(pmalloc_t *pm, pmalloc_item_t* node)`

Merge available nodes immediately before and after the given node, by scanning backward for consecutive nodes, then scanning forward to merge all consecutive nodes from that address.

### pmalloc_item_insert

`void pmalloc_item_insert(pmalloc_item_t **root, void *ptr)`

*Internal:* Insert the memory block at `ptr` and prefixed by a `pmalloc_item` struct into the specified block item chain.

### pmalloc_item_remove

`void pmalloc_item_remove(pmalloc_item_t **root, pmalloc_item_t *node)`

*Internal:* Remove the memory block at `ptr` and prefixed by a `pmalloc_item` struct from the specified block item chain.

### pmalloc_dump_stats (Debug build only)

`void pmalloc_dump_stats(pmalloc_t *pm)`

Dump the bookkeeping info and available and assigned chains for the given pmalloc_t.

## Caveats

`pmalloc` focuses on extreme minimalism, and does not include hardening or safety in code. For example, calling `pmalloc_free` with a block that was not previously allocated will lead to undefined behaviour. `pmalloc` is also not thread safe.

## Contributing

PRs and comments welcome. Please see the [Code Of Conduct](CODE_OF_CONDUCT.md) for community guidelines.

