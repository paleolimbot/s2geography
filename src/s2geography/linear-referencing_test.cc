#include "s2geography/linear-referencing.h"

#include <gtest/gtest.h>

#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/arrow_udf/arrow_udf_test_internal.h"

using namespace s2geography;

TEST(LinearReferencing, ArrowUdfLineLocatePoint) {
  auto udf = s2geography::arrow_udf::LineLocatePoint();

  ASSERT_NO_FATAL_FAILURE(TestInitArrowUDF(
      udf.get(), {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteArrowUDF(
      udf.get(), {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE,
      {{"LINESTRING (0 0, 0 1)"},
       {"POINT (0 0)", "POINT (0 0.5)", "POINT (0 1)", std::nullopt}},
      {}, out_array.get()));

  ASSERT_NO_FATAL_FAILURE(TestResultArrow(
      out_array.get(), NANOARROW_TYPE_DOUBLE, {0.0, 0.5, 1.0, std::nullopt}));
}
