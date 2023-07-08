#include <gtest/gtest.h>
#include <stdlib.h>

extern "C" {
  #include "pmalloc.h"
}

// Instantiate, check 
TEST(PMAllocTest, AllocSizeFree) {
  pmalloc_t pmblock;
  pmalloc_t *pm = &pmblock;

  pmalloc_init(pm);

  char buffer[65536];
  pmalloc_addblock(pm, &buffer, 65536);

  #ifdef DEBUG
    printf("AllocSizeFree: Initial:\n");
    pmalloc_dump_stats(pm);
  #endif

  uint32_t len[6] = { 150, 256, 512, 100, 1024, 65536 };
  void* mem[6];

  for(uint32_t i = 0; i<6; i++) {
    mem[i] = pmalloc_malloc(pm, len[i]);
  }

  #ifdef DEBUG
    printf("AllocSizeFree: Allocated:\n");
    pmalloc_dump_stats(pm);
  #endif

  EXPECT_EQ(mem[5], nullptr) << "pmalloc_malloc(65536) allocated when it should not have";

  for(uint8_t i = 0; i<5; i++) {
    EXPECT_EQ(pmalloc_sizeof(pm, mem[i]), len[i]) << "pmalloc_sizeof incorrectly reports size for block";

  }

  // Free All
  for(uint32_t i = 0; i<5; i++) pmalloc_free(pm, mem[i]);

  #ifdef DEBUG
    printf("AllocSizeFree: Freed:\n");
    pmalloc_dump_stats(pm);
  #endif
}

// Deliberately fail because of fragmentation
TEST(PMAllocTest, FragmentationTest) {
  pmalloc_t pmblock;
  pmalloc_t *pm = &pmblock;

  pmalloc_init(pm);

  char buffer[65536];
  pmalloc_addblock(pm, &buffer, 4096);

  uint32_t len[8] = { 512, 512, 512, 512, 512, 512, 512, 320 };
  void* mem[8];

  // Alloc the memory
  for(uint32_t i = 0; i<8; i++) {
    EXPECT_NE(mem[i] = pmalloc_malloc(pm, len[i]), (void*)NULL) << "pmalloc_malloc should pass";
  }

  #ifdef DEBUG
    printf("FragmentationTest: Allocated:\n");
    pmalloc_dump_stats(pm);
  #endif

  // Free 1,3,5
  pmalloc_free(pm, mem[1]);
  pmalloc_free(pm, mem[3]);
  pmalloc_free(pm, mem[5]);

  #ifdef DEBUG
    printf("FragmentationTest: Partially Freed:\n");
    pmalloc_dump_stats(pm);
  #endif

  EXPECT_EQ(pmalloc_malloc(pm, 1024), (void*)NULL) << "pmalloc_malloc should fail because of fragmentation";

  pmalloc_free(pm, mem[0]);
  pmalloc_free(pm, mem[2]);
  pmalloc_free(pm, mem[4]);
  pmalloc_free(pm, mem[6]);
  pmalloc_free(pm, mem[7]);

  #ifdef DEBUG
    printf("FragmentationTest: Freed:\n");
    pmalloc_dump_stats(pm);
  #endif
}

// Test realloc
TEST(PMAllocTest, ReallocTestMiddleChainLargetNoSpaceAfter) {
  pmalloc_t pmblock;
  pmalloc_t *pm = &pmblock;

  pmalloc_init(pm);

  char buffer[65536];
  pmalloc_addblock(pm, &buffer, 65536);

  uint32_t len[4] = { 100, 200, 300, 150 };
  void* mem[4];

  // Allocate memory blocks
  for (int i = 0; i < 4; ++i) {
    mem[i] = pmalloc_malloc(pm, len[i]);
    #ifdef DEBUG
      printf("mem[%d] = %016llx\n", i, (unsigned long long)(char*)mem[i]);
    #endif
  }

  #ifdef DEBUG
    printf("ReallocTestMiddleChainLargetNoSpaceAfter: Allocated:\n");
    pmalloc_dump_stats(pm);
  #endif

  // Reallocate the first block with a larger size
  uint32_t newSize = 250;
  mem[0] = pmalloc_realloc(pm, mem[0], newSize);

  #ifdef DEBUG
    printf("Realloc:\nmem[0] = %016llx\n", (unsigned long long)(char*)mem[0]);
  #endif

  #ifdef DEBUG
    printf("ReallocTestMiddleChainLargetNoSpaceAfter: After reallocating mem[0] to size %u:\n", newSize);
    pmalloc_dump_stats(pm);
  #endif

  EXPECT_EQ(pmalloc_sizeof(pm, mem[0]), newSize) << "pmalloc_sizeof incorrectly reports size for reallocated block";
}

// Reallocate a block in the middle of the chain with a smaller size
TEST(PMAllocTest, ReallocTestMiddleChainToSmallerSize) {
  pmalloc_t pmblock;
  pmalloc_t *pm = &pmblock;

  pmalloc_init(pm);

  char buffer[65536];
  pmalloc_addblock(pm, &buffer, 65536);

  uint32_t len[3] = { 100, 200, 300 };
  void* mem[3];

  // Allocate memory blocks
  for (int i = 0; i < 3; ++i) {
    mem[i] = pmalloc_malloc(pm, len[i]);
    #ifdef DEBUG
      printf("mem[%d] = %016llx\n", i, (unsigned long long)(char*)mem[i]);
    #endif
  }

  #ifdef DEBUG
    printf("ReallocTestMiddleChainToSmallerSize: Allocated:\n");
    pmalloc_dump_stats(pm);
  #endif

  uint32_t newSize = 150;
  mem[1] = pmalloc_realloc(pm, mem[1], newSize);
  EXPECT_NE(mem[1], (void*)NULL) << "pmalloc_realloc should not fail reallocating a smaller block";

  #ifdef DEBUG
    printf("ReallocTestMiddleChainToSmallerSize: After reallocating mem[1] to size %u:\n", newSize);
    pmalloc_dump_stats(pm);
  #endif

  EXPECT_EQ(pmalloc_sizeof(pm, mem[1]), newSize) << "pmalloc_sizeof incorrectly reports size for reallocated block";
}

// Test realloc
TEST(PMAllocTest, ReallocTestToSameSize) {
  pmalloc_t pmblock;
  pmalloc_t *pm = &pmblock;

  pmalloc_init(pm);

  char buffer[65536];
  pmalloc_addblock(pm, &buffer, 65536);

  uint32_t len[4] = { 100, 200, 300, 150 };
  void* mem[4];

  // Allocate memory blocks
  for (int i = 0; i < 4; ++i) {
    mem[i] = pmalloc_malloc(pm, len[i]);
    #ifdef DEBUG
      printf("mem[%d] = %016llx\n", i, (unsigned long long)(char*)mem[i]);
    #endif
  }

  // Reallocate the third block with the same size
  uint32_t newSize = len[2];
  mem[2] = pmalloc_realloc(pm, mem[2], newSize);

  #ifdef DEBUG
    printf("ReallocTestToSameSize: After reallocating mem[2] to size %u:\n", newSize);
    pmalloc_dump_stats(pm);
  #endif

  EXPECT_EQ(pmalloc_sizeof(pm, mem[2]), newSize) << "pmalloc_sizeof incorrectly reports size for reallocated block";
}

// Reallocate a block at the end of the allocated chain with space after to a larger size
TEST(PMAllocTest, ReallocTestEndOfChainWithSpaceAfter) {
  pmalloc_t pmblock;
  pmalloc_t *pm = &pmblock;

  pmalloc_init(pm);

  char buffer[65536];
  pmalloc_addblock(pm, &buffer, 65536);

  uint32_t len[4] = { 100, 200, 300, 150 };
  void* mem[4];

  // Allocate memory blocks
  for (int i = 0; i < 4; ++i) {
    mem[i] = pmalloc_malloc(pm, len[i]);
  }

  #ifdef DEBUG
    printf("ReallocTestEndOfChainWithSpaceAfter: Initial:\n");
    pmalloc_dump_stats(pm);
  #endif

  // 
  uint32_t newSize = 2048;
  mem[3] = pmalloc_realloc(pm, mem[3], newSize);

  #ifdef DEBUG
    printf("ReallocTestEndOfChainWithSpaceAfter: After reallocating mem[3] to size %u:\n", newSize);
    pmalloc_dump_stats(pm);
  #endif

  EXPECT_EQ(pmalloc_sizeof(pm, mem[3]), newSize) << "pmalloc_sizeof incorrectly reports size for reallocated block";
}

// Reallocate a block at in the middle of the allocated chain with space after to a larger size
TEST(PMAllocTest, ReallocTestMiddleChainWithSpaceAfter) {
  pmalloc_t pmblock;
  pmalloc_t *pm = &pmblock;

  pmalloc_init(pm);

  char buffer[65536];
  pmalloc_addblock(pm, &buffer, 65536);

  uint32_t len[4] = { 100, 200, 300, 150 };
  void* mem[4];

  // Allocate memory blocks
  for (int i = 0; i < 4; ++i) {
    mem[i] = pmalloc_malloc(pm, len[i]);
  }

  pmalloc_free(pm, mem[2]);

  #ifdef DEBUG
    printf("ReallocTestMiddleChainWithSpaceAfter: Initial:\n");
    pmalloc_dump_stats(pm);
  #endif

  uint32_t newSize = 450;
  mem[1] = pmalloc_realloc(pm, mem[1], newSize);

  #ifdef DEBUG
    printf("ReallocTestMiddleChainWithSpaceAfter: After reallocating mem[1] to size %u:\n", newSize);
    pmalloc_dump_stats(pm);
  #endif

  EXPECT_EQ(pmalloc_sizeof(pm, mem[1]), newSize) << "pmalloc_sizeof incorrectly reports size for reallocated block";
}

// Reallocate should fail when there's free space but not enough at the end of the memory
TEST(PMAllocTest, ReallocTestEndOfMemNoFreeSpace) {
  pmalloc_t pmblock;
  pmalloc_t *pm = &pmblock;

  pmalloc_init(pm);

  char buffer[4096];
  pmalloc_addblock(pm, &buffer, 4096);

  uint32_t len[2] = { 2048, 1024 };
  void* mem[2];

  // Allocate memory blocks
  for (int i = 0; i < 2; ++i) {
    mem[i] = pmalloc_malloc(pm, len[i]);
  }

  #ifdef DEBUG
    printf("ReallocTestEndOfMemNoFreeSpace: Initial:\n");
    pmalloc_dump_stats(pm);
  #endif

  uint32_t newSize = 2048;
  mem[1] = pmalloc_realloc(pm, mem[1], newSize);

  #ifdef DEBUG
    printf("ReallocTestEndOfMemNoFreeSpace: After reallocating mem[1] to size %u:\n", newSize);
    pmalloc_dump_stats(pm);
  #endif

  EXPECT_EQ(mem[1], (void*)NULL) << "pmalloc_realloc should return NULL on not enougb space";
}
