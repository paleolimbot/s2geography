#include "s2geography/predicates.h"

#include <gtest/gtest.h>

#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/sedona_udf/sedona_udf_test_internal.h"

TEST(Predicates, ArrowUdfIntersects) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::IntersectsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL,
      {{"POLYGON ((0 0, 1 0, 0 1, 0 0))"},
       {"POINT (0.25 0.25)", "POINT (-1 -1)", std::nullopt}},
      {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, std::nullopt}));
}

TEST(Predicates, ArrowUdfEquals) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::EqualsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL,
      {{"POLYGON ((0 0, 1 0, 0 1, 0 0))"},
       {"POLYGON ((1 0, 0 1, 0 0, 1 0))", "POLYGON ((0 0, 2 0, 0 2, 0 0))",
        std::nullopt}},
      {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, std::nullopt}));
}

TEST(Predicates, ArrowUdfContains) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::ContainsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL,
      {{"POLYGON ((0 0, 2 0, 0 2, 0 0))"},
       {"POINT (0.25 0.25)", "POINT (-1 -1)", std::nullopt}},
      {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, std::nullopt}));
}
