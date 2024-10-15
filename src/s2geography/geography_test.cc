#include <gtest/gtest.h>

#include "s2geography.h"
#include "s2geography/geography.h"

using namespace s2geography;

TEST(Geography, EmptyPolygon) {
  PolygonGeography geog;

  EXPECT_TRUE(geog.Polygon()->is_empty());
}
