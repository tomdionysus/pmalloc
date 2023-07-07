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

  uint32_t len[6] = { 150, 256, 512, 100, 1024, 65536 };
  void* mem[6];

  for(uint32_t i = 0; i<6; i++) mem[i] = pmalloc_malloc(pm, len[i]);

  EXPECT_EQ(mem[5], nullptr) << "pmalloc_malloc(65536) allocated when it should not have";

  for(uint8_t i = 0; i<5; i++) {
    EXPECT_EQ(pmalloc_sizeof(pm, mem[i]), len[i]) << "pmalloc_sizeof incorrectly reports size for block";
  }

  for(uint32_t i = 0; i<5; i++) pmalloc_free(pm, mem[i]);
}

// Test freeing block
TEST(PMAllocTest, FreeBlock) {
  pmalloc_t pmblock;
  pmalloc_t *pm = &pmblock;

  pmalloc_init(pm);

  char buffer[65536];
  pmalloc_addblock(pm, &buffer, 65535);

  uint32_t len[4] = { 1024, 2048 };
  void* mem[4];

  for(uint32_t i = 0; i<4; i++) mem[i] = pmalloc_malloc(pm, len[i]);

  #ifdef DEBUG
  pmalloc_dump_stats(pm);
  #endif

  EXPECT_EQ(pmalloc_usedmem(pm), 1024+2048);
  EXPECT_EQ(pmalloc_overheadmem(pm), sizeof(pmalloc_item)*2);
  EXPECT_EQ(pmalloc_freemem(pm), 65536-1024-2048-sizeof(pmalloc_item)*4);

  for(uint32_t i = 0; i<2; i++) pmalloc_free(pm, mem[i]);
}
