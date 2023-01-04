# pmalloc

pmalloc - A Heap based memory manager

## Description

pmalloc can be used to manage memory using a familiar malloc/free pattern in embedded systems, or to manage sub-allocation within an area of memory given by the OS.

To get started, call `paddblock` with the address and size of the memory to be used for allocation.
