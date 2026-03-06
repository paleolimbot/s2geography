#include <gtest/gtest.h>

#include "s2geography/wkb-geography.h"

using namespace s2geography;

TEST(GeoArrowLaxPolylineShape, DefaultConstructor) {
  GeoArrowLaxPolylineShape shape;
  EXPECT_EQ(shape.num_edges(), 0);
  EXPECT_EQ(shape.dimension(), 1);
  EXPECT_EQ(shape.num_chains(), 0);
}
