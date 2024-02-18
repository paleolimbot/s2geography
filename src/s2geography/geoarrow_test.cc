
#include <gtest/gtest.h>

#include "s2geography.h"

TEST(GeoArrow, GeoArrowVersionTest) {
  EXPECT_STREQ(s2geography::geoarrow::version(), "0.2.0-SNAPSHOT");
}
