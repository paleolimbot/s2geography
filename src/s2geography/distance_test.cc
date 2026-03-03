#include "s2geography/distance.h"

#include <gtest/gtest.h>

#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/sedona_udf/sedona_udf_test_internal.h"

using namespace s2geography;

TEST(Distance, PointDistance) {
  WKTReader reader;
  auto geog1 = reader.read_feature("POINT (0 0)");
  auto geog2 = reader.read_feature("POINT (90 0)");
  ShapeIndexGeography geog1_index(*geog1);
  ShapeIndexGeography geog2_index(*geog2);

  EXPECT_DOUBLE_EQ(s2_distance(geog1_index, geog2_index), M_PI / 2);
}

TEST(Distance, SedonaUdfDistance) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::DistanceKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE,
      {{"POINT (0 0)"}, {"POINT (0 1)", "LINESTRING (0 0, 0 1)", std::nullopt}},
      {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array.get(), NANOARROW_TYPE_DOUBLE,
                      {111195.10117748393, 0.0, std::nullopt}));
}

TEST(Distance, SedonaUdfMaxDistance) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::MaxDistanceKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE,
      {{"POINT (0 0)"}, {"POINT (0 1)", "LINESTRING (0 0, 0 1)", std::nullopt}},
      {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array.get(), NANOARROW_TYPE_DOUBLE,
                      {111195.10117748393, 111195.10117748393, std::nullopt}));
}

TEST(Distance, SedonaUdfShortestLine) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::ShortestLineKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, ARROW_TYPE_WKB,
      {{"POINT (0 0)"}, {"POINT (0 1)", "LINESTRING (0 0, 0 1)", std::nullopt}},
      {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(),
      {"LINESTRING (0 0, 0 1)", "LINESTRING (0 0, 0 0)", std::nullopt}));
}

TEST(Distance, SedonaUdfClosestPoint) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::ClosestPointKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, ARROW_TYPE_WKB,
      {{"POINT (0 1)", "LINESTRING (0 0, 0 1)", std::nullopt}, {"POINT (0 0)"}},
      {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 1)", "POINT (0 0)", std::nullopt}));
}
