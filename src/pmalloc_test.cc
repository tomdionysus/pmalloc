#include <gtest/gtest.h>

extern "C" {
  #include "pmalloc.h"
}

// Instantiate, check 
TEST(FreemapTest, NewDestroy) {
  freemap_t *map = freemap_new(127);

  EXPECT_NE(map->bitmap, nullptr);
  EXPECT_EQ(map->total, 127);
  EXPECT_EQ(map->free, 127);

  freemap_destroy(map);
}
