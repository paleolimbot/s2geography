
#include <gtest/gtest.h>

#include "s2geography.h"

namespace s2geography {

const char* s2_geoarrow_version();

}

TEST(GeoArrow, GeoArrowVersionTest) {
  EXPECT_STREQ(s2geography::s2_geoarrow_version(), "0.2.0-SNAPSHOT");
}
