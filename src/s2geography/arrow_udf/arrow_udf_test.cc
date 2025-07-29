#include <gtest/gtest.h>

#include "geoarrow/geoarrow.hpp"
#include "nanoarrow/nanoarrow.hpp"
#include "s2geography.h"
#include "s2geography/arrow_udf/arrow_udf_test_internal.h"

TEST(ArrowUdf, Intersects) {
  auto udf = s2geography::arrow_udf::Intersects();

  ASSERT_NO_FATAL_FAILURE(TestInitArrowUDF(
      udf.get(), {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteArrowUDF(
      udf.get(), {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL,
      {{"POLYGON ((0 0, 1 0, 0 1, 0 0))"},
       {"POINT (0.25 0.25)", "POINT (-1 -1)", std::nullopt}},
      {}, out_array.get()));

  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, ARROW_TYPE_WKB}));
}
