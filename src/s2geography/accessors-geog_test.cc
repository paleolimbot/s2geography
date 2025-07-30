#include "s2geography/accessors-geog.h"

#include <gtest/gtest.h>

#include "geoarrow/geoarrow.hpp"
#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/arrow_udf/arrow_udf_test_internal.h"

TEST(ArrowUdf, Length) {
  auto udf = s2geography::arrow_udf::Length();

  ASSERT_NO_FATAL_FAILURE(
      TestInitArrowUDF(udf.get(), {ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteArrowUDF(udf.get(), {ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE,
                          {{"POINT (0 1)", "LINESTRING (0 0, 0 1)",
                            "POLYGON ((0 0, 0 1, 1 0, 0 0))", std::nullopt}},
                          {}, out_array.get()));

  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array.get(), NANOARROW_TYPE_DOUBLE,
                      {0.0, 111195.10117748393, 0.0, ARROW_TYPE_WKB}));
}

TEST(ArrowUdf, Centroid) {
  auto udf = s2geography::arrow_udf::Centroid();

  ASSERT_NO_FATAL_FAILURE(
      TestInitArrowUDF(udf.get(), {ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteArrowUDF(udf.get(), {ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE,
                          {{"POINT (0 1)", "LINESTRING (0 0, 0 1)",
                            "POLYGON ((0 0, 0 1, 1 0, 0 0))", std::nullopt}},
                          {}, out_array.get()));

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 1)", "POINT (0 0.5)",
                        "POINT (0.33335 0.333344)", ARROW_TYPE_WKB}));
}

TEST(ArrowUdf, InterpolateNormalized) {
  auto udf = s2geography::arrow_udf::InterpolateNormalized();

  ASSERT_NO_FATAL_FAILURE(TestInitArrowUDF(
      udf.get(), {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteArrowUDF(udf.get(), {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
                          ARROW_TYPE_WKB, {{"LINESTRING (0 0, 0 1)"}},
                          {{0.0, 0.5, 1.0, ARROW_TYPE_WKB}}, out_array.get()));

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(),
      {"POINT (0 0)", "POINT (0 0.5)", "POINT (0 1)", ARROW_TYPE_WKB}));
}
