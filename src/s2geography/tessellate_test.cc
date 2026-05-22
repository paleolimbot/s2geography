#include "s2geography/tessellate.h"

#include <gtest/gtest.h>

#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/sedona_udf/sedona_udf_test_internal.h"

using namespace s2geography;

TEST(Tessellate, SedonaUdfTessellateToGeogArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::TessellateToGeog(&kernel);
  struct SedonaCScalarKernelImpl impl;
  // TessellateToGeog takes geometry (PLANAR) input, outputs geography
  // (SPHERICAL)
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB_PLANAR, NANOARROW_TYPE_DOUBLE},
      ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  // Use a very large tolerance (1e9 meters) so no tessellation occurs
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB_PLANAR, NANOARROW_TYPE_DOUBLE},
      {{"POINT (0 0)", "LINESTRING (0 0, 1 1)", std::nullopt}},
      {{1e9, 1e9, 1e9}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  // With large tolerance, output should match input (just converted to
  // geography)
  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 0)", "LINESTRING (0 0, 1 1)", std::nullopt}));
}

TEST(Tessellate, SedonaUdfTessellateToGeomArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::TessellateToGeom(&kernel);
  struct SedonaCScalarKernelImpl impl;
  // TessellateToGeom takes geography (SPHERICAL) input, outputs geometry
  // (PLANAR)
  ASSERT_NO_FATAL_FAILURE(
      TestInitKernel(&kernel, &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
                     ARROW_TYPE_WKB_PLANAR));

  nanoarrow::UniqueArray out_array;
  // Use a very large tolerance (1e9 meters) so no tessellation occurs
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
      {{"POINT (0 0)", "LINESTRING (0 0, 1 1)", std::nullopt}},
      {{1e9, 1e9, 1e9}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  // With large tolerance, output should match input (just converted to
  // geometry)
  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 0)", "LINESTRING (0 0, 1 1)", std::nullopt}));
}

TEST(Tessellate, SedonaUdfSegmentizeArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::Segmentize(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  // Use a very large segment length (1e9 meters) so no segmentization occurs
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
      {{"POINT (0 0)", "LINESTRING (0 0, 1 1)", std::nullopt}},
      {{1e9, 1e9, 1e9}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  // With large segment length, output should match input
  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 0)", "LINESTRING (0 0, 1 1)", std::nullopt}));
}

TEST(Tessellate, SedonaUdfSegmentizeWithSubdivision) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::Segmentize(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  // Use a small segment length to force subdivision
  // 111320 meters is approximately 1 degree at the equator
  // (Earth radius ~6371km, so 1 degree = 6371000 * pi/180 ≈ 111195m)
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
      {{"LINESTRING (0 0, 0 2)"}}, {{111320.0}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  // With ~1 degree max segment, a 2 degree line should be split into 2
  // segments
  ASSERT_NO_FATAL_FAILURE(
      TestResultGeography(out_array.get(), {"LINESTRING (0 0, 0 1, 0 2)"}));
}
