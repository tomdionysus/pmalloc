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
