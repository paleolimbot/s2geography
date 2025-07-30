#include "s2geography/distance.h"

#include <gtest/gtest.h>

#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/arrow_udf/arrow_udf_test_internal.h"

using namespace s2geography;

TEST(Distance, PointDistance) {
  WKTReader reader;
  auto geog1 = reader.read_feature("POINT (0 0)");
  auto geog2 = reader.read_feature("POINT (90 0)");
  ShapeIndexGeography geog1_index(*geog1);
  ShapeIndexGeography geog2_index(*geog2);

  EXPECT_DOUBLE_EQ(s2_distance(geog1_index, geog2_index), M_PI / 2);
}

TEST(Distance, ArrowUdfDistance) {
  auto udf = s2geography::arrow_udf::Distance();

  ASSERT_NO_FATAL_FAILURE(TestInitArrowUDF(
      udf.get(), {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteArrowUDF(
      udf.get(), {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE,
      {{"POINT (0 0)"}, {"POINT (0 1)", "LINESTRING (0 0, 0 1)", std::nullopt}},
      {}, out_array.get()));

  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array.get(), NANOARROW_TYPE_DOUBLE,
                      {111195.10117748393, 0.0, ARROW_TYPE_WKB}));
}

TEST(Distance, ArrowUdfMaxDistance) {
  auto udf = s2geography::arrow_udf::MaxDistance();

  ASSERT_NO_FATAL_FAILURE(TestInitArrowUDF(
      udf.get(), {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteArrowUDF(
      udf.get(), {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE,
      {{"POINT (0 0)"}, {"POINT (0 1)", "LINESTRING (0 0, 0 1)", std::nullopt}},
      {}, out_array.get()));

  ASSERT_NO_FATAL_FAILURE(TestResultArrow(
      out_array.get(), NANOARROW_TYPE_DOUBLE,
      {111195.10117748393, 111195.10117748393, ARROW_TYPE_WKB}));
}
