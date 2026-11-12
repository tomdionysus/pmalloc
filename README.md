# pmalloc

[![Release](https://img.shields.io/github/v/release/tomdionysus/pmalloc?display_name=release&label=latest)](https://github.com/tomdionysus/pmalloc/releases/latest)
[![CI](https://github.com/tomdionysus/pmalloc/actions/workflows/ci.yml/badge.svg)](https://github.com/tomdionysus/pmalloc/actions/workflows/ci.yml)
[![CodeQL](https://github.com/tomdionysus/pmalloc/actions/workflows/codeql.yml/badge.svg)](https://github.com/tomdionysus/pmalloc/actions/workflows/codeql.yml)
[![codecov](https://codecov.io/gh/tomdionysus/pmalloc/branch/main/graph/badge.svg)](https://codecov.io/gh/tomdionysus/pmalloc/tree/main)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A minimal C library to manage memory with a familiar malloc/free pattern, on embedded systems
with no heap of their own, or to sub-allocate within a block of memory given by the OS.

- Two files, `src/pmalloc.h` and `src/pmalloc.c`, in plain C99 with no dependencies.
- Under 1 KB of code on a Cortex-M (784 bytes on Cortex-M0, `-Os`).
- `malloc`, `calloc`, `realloc` and `free`, with every returned pointer aligned to a configurable
  boundary.
- Any number of separate memory regions in one heap.

## Getting started

### 1. Add pmalloc to your project

**Copy the files.** Add `src/pmalloc.h` and `src/pmalloc.c` to your build. This suits most
embedded toolchains.

**Or use CMake.** With pmalloc in a subdirectory, or fetched:

```cmake
include(FetchContent)
FetchContent_Declare(pmalloc GIT_REPOSITORY https://github.com/tomdionysus/pmalloc.git GIT_TAG 0.3.0)
FetchContent_MakeAvailable(pmalloc)

target_link_libraries(your_target PRIVATE pmalloc::pmalloc)
```

As a subproject, pmalloc builds only the library, not its tests or examples.

**Or install it** and use `find_package`:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /your/prefix
```

```cmake
find_package(pmalloc 0.3 CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE pmalloc::pmalloc)
```

### 2. Give it memory and allocate

```c
#include "pmalloc.h"

static pmalloc_t heap;
static unsigned char memory[16384];  // or a fixed address and size on your board

int main(void) {
  pmalloc_init(&heap);
  pmalloc_addblock(&heap, memory, sizeof(memory));

  char *buffer = pmalloc_malloc(&heap, 256);
  if (buffer == NULL) return 1;  // out of memory

  char *bigger = pmalloc_realloc(&heap, buffer, 512);
  if (bigger != NULL) buffer = bigger;  // on failure, buffer is still valid

  pmalloc_free(&heap, buffer);
  return 0;
}
```

There are full programs in [example](example): [basic](example/example_basic.c) and
[suballocation](example/example_suballocation.c).

### 3. Set the alignment, if you need to

Every pointer pmalloc returns is aligned to `PMALLOC_ALIGN` bytes, which defaults to the size of
a pointer. If you store `double` or `uint64_t` on a 32-bit target, or SIMD types, raise it:

```bash
cc -DPMALLOC_ALIGN=8 -c src/pmalloc.c       # copying the files
cmake -S . -B build -DPMALLOC_ALIGN=16      # with CMake
```

It must be a power of two, at least the size of a pointer.

### 4. Turn on checks while you develop

Build with `PMALLOC_CHECKS` defined (`-DPMALLOC_CHECKS=ON` with CMake) and pmalloc refuses double
frees, pointers it did not return and overlapping regions instead of corrupting the heap, and calls
a hook of yours so you can stop on the bug. It costs about 130 bytes of code and a list walk per
call, and nothing at all when left off. See [PMALLOC_CHECKS](docs/API.md#pmalloc_checks).

## Documentation

The [API reference](docs/API.md) covers every function, the memory layout, the configuration and
the caveats.

## Building and testing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPMALLOC_WARNINGS_AS_ERRORS=ON
cmake --build build -j
./build/pmalloc_tests
```

The tests use [Unity](https://github.com/ThrowTheSwitch/Unity), vendored in `test/vendor/unity`,
so nothing is downloaded. A `Debug` build also enables `pmalloc_dump_stats`.

| Option | Default | Effect |
|---|---|---|
| `PMALLOC_ALIGN` | pointer size | Alignment of every block and returned pointer |
| `PMALLOC_CHECKS` | OFF | Validate every pointer and region passed in |
| `PMALLOC_BUILD_TESTING` | ON when top level | Build `pmalloc_tests` |
| `PMALLOC_BUILD_EXAMPLES` | ON when top level | Build the examples |
| `PMALLOC_WARNINGS_AS_ERRORS` | OFF | Treat compiler warnings as errors |
| `PMALLOC_COVERAGE` | OFF | Instrument for coverage with GCC or Clang |

To measure coverage locally with GCC and [gcovr](https://gcovr.com):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DPMALLOC_COVERAGE=ON
cmake --build build -j
./build/pmalloc_tests
gcovr --root . --filter src/ --object-directory build
```

## Caveats

pmalloc is built for minimalism. Unless built with `PMALLOC_CHECKS`, it does not defend against
misuse: freeing a pointer it did not allocate or freeing twice corrupts the heap, and writing past
a block always does. It is not thread safe. Sizes are 32-bit. See
[Caveats](docs/API.md#caveats).

## Contributing

PRs and comments welcome. Please see the [Code Of Conduct](CODE_OF_CONDUCT.md) for community
guidelines. Changes are listed in the [changelog](CHANGELOG.md).

## License

MIT. See [LICENSE.txt](LICENSE.txt). The vendored Unity test framework is MIT licensed by its
authors; see [test/vendor/unity/LICENSE.txt](test/vendor/unity/LICENSE.txt).
