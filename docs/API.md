# pmalloc API reference

pmalloc manages one or more regions of memory you give it, handing out blocks with a
malloc/free pattern. Everything lives in `src/pmalloc.h` and `src/pmalloc.c`; there are no other
dependencies, and no use of the C library except `printf` in a `DEBUG` build.

- [How memory is laid out](#how-memory-is-laid-out)
- [Compile-time configuration](#compile-time-configuration)
- [Types](#types)
- [Functions](#functions)
- [Internal functions](#internal-functions)
- [Debug build](#debug-build)
- [Caveats](#caveats)

## How memory is laid out

Every block, free or allocated, is a header followed by its payload:

```
| header (pmalloc_item_t, padded) | payload (size rounded up to PMALLOC_ALIGN) |
```

- The header is a `pmalloc_item_t` padded to a multiple of `PMALLOC_ALIGN`. With the default
  alignment that is 24 bytes on a 64-bit target and 12 bytes on a 32-bit target.
- Each payload occupies its size rounded up to `PMALLOC_ALIGN`, so every header and every pointer
  pmalloc returns is aligned to `PMALLOC_ALIGN`.
- Free blocks and allocated blocks are kept on two chains, each in address order. Allocation is
  first fit: the lowest-addressed free block big enough is used, and what is left over becomes a
  new free block when it can hold a header and at least `PMALLOC_ALIGN` bytes. A smaller
  remainder stays with the allocated block.
- Freeing a block merges it with any free block directly before or after it, so no two free
  blocks are ever adjacent.
- Regions added with `pmalloc_addblock` are never merged with each other.

The memory cost of an allocation of `n` bytes is one header plus `n` rounded up to
`PMALLOC_ALIGN`.

## Compile-time configuration

### PMALLOC_ALIGN

The alignment, in bytes, of every block header and every returned pointer. It must be a power of
two and at least `sizeof(void *)`; anything else is a compile error. The default is
`sizeof(void *)`.

Raise it when the memory will hold types with stricter alignment, for example 8 for `double` or
`uint64_t` on a 32-bit target, or 16 for SIMD types:

```bash
cc -DPMALLOC_ALIGN=16 -c src/pmalloc.c
```

or with CMake:

```bash
cmake -S . -B build -DPMALLOC_ALIGN=16
```

Build `pmalloc.c` and any code that reads `PMALLOC_ALIGN` with the same value. The CMake option
adds the definition to the `pmalloc` target's public interface, so targets that link it see the
same value.

### PMALLOC_VERSION

`PMALLOC_VERSION_MAJOR`, `PMALLOC_VERSION_MINOR` and `PMALLOC_VERSION_PATCH` are integers, and
`PMALLOC_VERSION` is the same version as a string, for example `"0.2.0"`.

## Types

### pmalloc_t

```c
typedef struct pmalloc {
    pmalloc_item_t *available;  // The linked list of available blocks
    pmalloc_item_t *assigned;   // The linked list of allocated blocks
    uint32_t freemem;           // The current free memory count
    uint32_t totalmem;          // The total available free memory
    uint32_t totalnodes;        // The number of nodes in the allocated list
} pmalloc_t;
```

The root of one heap. Allocate it anywhere (static, stack or global), call `pmalloc_init` on it,
and pass it to every other call. Treat its fields as read-only; use the statistics functions to
read them.

### pmalloc_item_t

```c
typedef struct pmalloc_item {
    struct pmalloc_item *prev;  // The previous block in the chain
    struct pmalloc_item *next;  // The next block in the chain
    uint32_t size;              // This is the size of the block as reported to the user
} pmalloc_item_t;
```

The header in front of every block.

## Functions

### pmalloc_init

```c
void pmalloc_init(pmalloc_t *pm);
```

Initialise `pm` as an empty heap. Call it once before any other function. Calling it again
discards everything the heap knew about without touching the memory.

### pmalloc_addblock

```c
void pmalloc_addblock(pmalloc_t *pm, void *ptr, uint32_t size);
```

Give the `size` bytes at `ptr` to the heap. The region does not need to be aligned: pmalloc uses
the part of it between its first and last `PMALLOC_ALIGN` boundaries.

- A NULL `ptr`, or a region too small for a header and `PMALLOC_ALIGN` bytes, is ignored.
- Call it as many times as you like to add more regions; `totalmem` grows by each region's usable
  size.
- When a region directly touches memory already in the heap, the first or last `PMALLOC_ALIGN`
  bytes of the new region are left unused so the two are never merged.
- Do not add the same memory twice, or memory that overlaps a region already added.

### pmalloc_malloc

```c
void *pmalloc_malloc(pmalloc_t *pm, uint32_t size);
```

Allocate a block of at least `size` bytes. Returns a pointer aligned to `PMALLOC_ALIGN`, or NULL
if no free block is big enough, or if `size` rounded up to `PMALLOC_ALIGN` does not fit in 32 bits.

`size` may be 0; the result is a distinct, valid pointer to a zero-sized block that must be freed
like any other.

### pmalloc_calloc

```c
void *pmalloc_calloc(pmalloc_t *pm, uint32_t num, uint32_t size);
```

Allocate a block of `num * size` bytes and fill it with zeros. Returns NULL if `num * size`
overflows 32 bits or if `pmalloc_malloc` would return NULL.

### pmalloc_realloc

```c
void *pmalloc_realloc(pmalloc_t *pm, void *ptr, uint32_t size);
```

Resize the block at `ptr` to `size` bytes and return a pointer to it.

- If `ptr` is NULL, it behaves like `pmalloc_malloc(pm, size)`.
- Shrinking, or growing within the block's current rounded size, is done in place and returns
  `ptr`. The space released becomes a free block when it can hold a header and `PMALLOC_ALIGN`
  bytes.
- Growing is done in place when the block is followed directly by a free block big enough to
  cover the difference, and returns `ptr`.
- Otherwise a new block is allocated, the contents are copied, the old block is freed, and the new
  pointer is returned. This costs a copy of the whole block.
- If no block can be found, it returns NULL and the original block is untouched and still
  allocated.
- If `size` rounded up to `PMALLOC_ALIGN` does not fit in 32 bits, it returns NULL and changes
  nothing.

### pmalloc_free

```c
void pmalloc_free(pmalloc_t *pm, void *ptr);
```

Free the block at `ptr`, which must have come from this heap and not already been freed. Freeing
NULL does nothing.

### pmalloc_sizeof

```c
uint32_t pmalloc_sizeof(pmalloc_t *pm, void *ptr);
```

Return the usable size of the block at `ptr`. This is the size most recently asked for in
`pmalloc_malloc`, `pmalloc_calloc` or `pmalloc_realloc`, except when the block kept a remainder
too small to become a free block; then it is the block's whole usable size, which is larger than
the size asked for by less than one header and two `PMALLOC_ALIGN` units. Every byte up to the
returned size may be used.

Returns 0 for NULL.

### pmalloc_freemem

```c
uint32_t pmalloc_freemem(pmalloc_t *pm);
```

The total bytes in free blocks. Because memory can be fragmented, the largest single allocation
that will succeed may be smaller.

### pmalloc_totalmem

```c
uint32_t pmalloc_totalmem(pmalloc_t *pm);
```

The usable bytes in all regions added: each region's aligned size less one header.

### pmalloc_usedmem

```c
uint32_t pmalloc_usedmem(pmalloc_t *pm);
```

`totalmem - freemem`: the bytes in allocated blocks, rounded up to `PMALLOC_ALIGN`, plus the
headers created by splitting regions into blocks. It is 0 when everything has been freed.

### pmalloc_overheadmem

```c
uint32_t pmalloc_overheadmem(pmalloc_t *pm);
```

The bytes currently taken by block headers: one padded header per block, free or allocated.

## Internal functions

These are declared in the header for testing and debugging. Applications should not need them.

### pmalloc_merge

```c
void pmalloc_merge(pmalloc_t *pm, pmalloc_item_t *node);
```

Merge the free block `node` with the run of adjacent free blocks around it, returning each merged
header to `freemem`. `node` must be on the available chain.

### pmalloc_item_insert

```c
void pmalloc_item_insert(pmalloc_item_t **root, void *ptr);
```

Insert the header at `ptr` into the chain at `*root`, keeping it in address order. Both of the
inserted header's links are set; whatever they held before is ignored.

### pmalloc_item_remove

```c
void pmalloc_item_remove(pmalloc_item_t **root, pmalloc_item_t *node);
```

Unlink `node` from the chain at `*root`, moving `*root` if `node` was first, and clear `node`'s
links.

## Debug build

When pmalloc is compiled with `DEBUG` defined (the CMake `Debug` build type does this for its own
targets), `pmalloc_init` prints a line, and this function is available:

### pmalloc_dump_stats

```c
void pmalloc_dump_stats(pmalloc_t *pm);
```

Print the heap's counters and every block on both chains, with its header address, payload start,
end, total bytes, header bytes and size.

## Caveats

- pmalloc does no hardening. Freeing a pointer that did not come from the heap, freeing twice, or
  writing past the end of a block corrupts the heap.
- pmalloc is not thread safe. Guard a shared `pmalloc_t` with your own lock, or give each thread
  its own.
- Sizes are 32-bit. A single region or allocation cannot exceed 4 GiB, and `totalmem` must fit in
  32 bits.
