#include <gtest/gtest.h>
#include <stdlib.h>

extern "C" {
  #include "pmalloc.h"
}

// Instantiate, check 
TEST(PMAllocTest, NewDestroy) {
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
