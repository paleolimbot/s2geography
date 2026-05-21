#include "s2geography/tessellate.h"

#include <gtest/gtest.h>

namespace s2geography {

TEST(Geometry, ToGeographyKernelExists) {
  struct SedonaCScalarKernel kernel;
  sedona_udf::TessellateToGeog(&kernel);
  EXPECT_STREQ(kernel.function_name(&kernel), "st_to_geography");
  kernel.release(&kernel);
}

TEST(Geometry, ToGeometryKernelExists) {
  struct SedonaCScalarKernel kernel;
  sedona_udf::TessellateToGeom(&kernel);
  EXPECT_STREQ(kernel.function_name(&kernel), "st_to_geometry");
  kernel.release(&kernel);
}

}  // namespace s2geography
