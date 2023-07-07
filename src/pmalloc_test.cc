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
    #ifdef DEBUG
      printf("mem[%d] = %016llx\n", i, (unsigned long long)(char*)mem[i]);
    #endif
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
    #ifdef DEBUG
      printf("mem[%d] = %016llx\n", i, (unsigned long long)(char*)mem[i]);
    #endif
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
TEST(PMAllocTest, ReallocTest) {
  pmalloc_t pmblock;
  pmalloc_t *pm = &pmblock;

  pmalloc_init(pm);

  char buffer[65536];
  pmalloc_addblock(pm, &buffer, 65536);

  #ifdef DEBUG
    printf("ReallocTest: Initial:\n");
    pmalloc_dump_stats(pm);
  #endif

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
    printf("ReallocTest: Allocated:\n");
    pmalloc_dump_stats(pm);
  #endif

  // Reallocate the first block with a larger size
  uint32_t newSize = 250;
  mem[0] = pmalloc_realloc(pm, mem[0], newSize);

  #ifdef DEBUG
    printf("Realloc:\nmem[0] = %016llx\n", (unsigned long long)(char*)mem[0]);
  #endif

  #ifdef DEBUG
    printf("ReallocTest: After reallocating mem[0] to size %u:\n", newSize);
    pmalloc_dump_stats(pm);
  #endif

  EXPECT_EQ(pmalloc_sizeof(pm, mem[0]), newSize) << "pmalloc_sizeof incorrectly reports size for reallocated block";

  // Reallocate the second block with a smaller size
  newSize = 150;
  mem[1] = pmalloc_realloc(pm, mem[1], newSize);

  #ifdef DEBUG
    printf("ReallocTest: After reallocating mem[1] to size %u:\n", newSize);
    pmalloc_dump_stats(pm);
  #endif

  EXPECT_EQ(pmalloc_sizeof(pm, mem[1]), newSize) << "pmalloc_sizeof incorrectly reports size for reallocated block";

  // Reallocate the third block with the same size
  newSize = len[2];
  mem[2] = pmalloc_realloc(pm, mem[2], newSize);

  #ifdef DEBUG
    printf("ReallocTest: After reallocating mem[2] to size %u:\n", newSize);
    pmalloc_dump_stats(pm);
  #endif

  EXPECT_EQ(pmalloc_sizeof(pm, mem[2]), newSize) << "pmalloc_sizeof incorrectly reports size for reallocated block";
}
