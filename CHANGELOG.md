# Changelog

## 0.2.0

No function, type or declaration has changed. Code written against 0.1.0 compiles and links
unchanged.

### Fixed

- `pmalloc_addblock` added the running free total to `totalmem` instead of the region's size, so
  `totalmem` and `pmalloc_usedmem` were wrong after a second region.
- Allocating into a hole between allocated blocks cut the allocated chain short, which could lead
  `pmalloc_realloc` to treat a live block as free space.
- Splitting a free block whose remainder was smaller than a header wrapped the remainder's size
  and the free-memory count around to about 4 GiB.
- Inserting into a chain could leave stale links on the inserted block.
- `pmalloc_realloc` growing in place could read past the end of a region, or write a free block
  header over the next allocated block.
- `pmalloc_calloc` did not check `num * size` for overflow.
- Sizes within `PMALLOC_ALIGN` of 4 GiB wrapped when rounded up.
- `pmalloc_addblock` did not check for NULL or a region too small to use.
- The suballocation example copied a string into a block one byte too small.

### Changed

- Alignment is configurable with `PMALLOC_ALIGN`, by default the size of a pointer as before.
  Block headers are padded to it, so any power of two at least the size of a pointer works.
- `pmalloc_sizeof` reports the size asked for again, as it did before alignment was added, rather
  than the size rounded up to the alignment. When a block kept a remainder too small to become a
  free block, it reports the block's whole usable size instead.
- A region added directly against memory already in the heap gives up `PMALLOC_ALIGN` bytes at
  the touching edge, so regions are never merged.
- `pmalloc_sizeof(NULL)` returns 0.

### Added

- `PMALLOC_ALIGN`, set at compile time or with the CMake option of the same name.
- `PMALLOC_VERSION`, `PMALLOC_VERSION_MAJOR`, `PMALLOC_VERSION_MINOR` and `PMALLOC_VERSION_PATCH`.
- `extern "C"` guards in `pmalloc.h`.
- A CMake `pmalloc::pmalloc` target, install rules and a `find_package` config.
- CMake options `PMALLOC_BUILD_TESTING`, `PMALLOC_BUILD_EXAMPLES`, `PMALLOC_WARNINGS_AS_ERRORS` and
  `PMALLOC_COVERAGE`.
- The API reference in `docs/API.md`.

### Build and tests

- The project is C only. Tests use Unity, vendored in `test/vendor/unity`, instead of downloading
  GoogleTest. The test binary is `pmalloc_tests`.
- The tests cover 100% of lines and branches. They run under GitHub Actions on Linux (GCC and
  Clang, 64-bit and 32-bit, several alignments) and macOS, with a Cortex-M cross-compile, Codecov
  upload and CodeQL analysis. CircleCI is removed.

## 0.1.0

The state of the library before version numbers were introduced.
