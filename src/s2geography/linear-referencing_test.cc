#include "s2geography/linear-referencing.h"

#include <gtest/gtest.h>

#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/sedona_udf/sedona_udf_test_internal.h"

using namespace s2geography;

TEST(LinearReferencing, SedonaUdfLineLocatePoint) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::LineLocatePointKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                        {{"LINESTRING (0 0, 0 1, 0 2)"},
                         {"POINT (0 0)", "POINT (0 0.5)", "POINT (0 1)",
                          "POINT (0 1.5)", "POINT (0 2)", std::nullopt}},
                        {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array.get(), NANOARROW_TYPE_DOUBLE,
                      {0.0, 0.25, 0.5, 0.75, 1.0, std::nullopt}));
}

TEST(LinearReferencing, SedonaUdfLineInterpolatePoint) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::LineInterpolatePointKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
      {{"LINESTRING (0 0, 0 1, 0 2)"}},
      {{0.0, 0.25, 0.5, 0.75, 1.0, std::nullopt}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 0)", "POINT (0 0.5)", "POINT (0 1)",
                        "POINT (0 1.5)", "POINT (0 2)", std::nullopt}));
}
