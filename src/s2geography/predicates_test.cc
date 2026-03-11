#include "s2geography/predicates.h"

#include <gtest/gtest.h>

#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/sedona_udf/sedona_udf_test_internal.h"

TEST(Predicates, SedonaUdfIntersectsScalarArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::IntersectsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                        {{"POLYGON ((0 0, 1 0, 0 1, 0 0))"},
                         {"POINT (0.25 0.25)", "POINT (-1 -1)", std::nullopt}},
                        {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, std::nullopt}));
}

TEST(Predicates, SedonaUdfIntersectsArrayScalar) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::IntersectsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                        {{"POINT (0.25 0.25)", "POINT (-1 -1)", std::nullopt},
                         {"POLYGON ((0 0, 1 0, 0 1, 0 0))"}},
                        {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, std::nullopt}));
}

TEST(Predicates, SedonaUdfEqualsScalarArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::EqualsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                        {{"POLYGON ((0 0, 1 0, 0 1, 0 0))"},
                         {"POLYGON ((1 0, 0 1, 0 0, 1 0))",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", std::nullopt}},
                        {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, std::nullopt}));
}

TEST(Predicates, SedonaUdfContainsScalarArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::ContainsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                        {{"POLYGON ((0 0, 2 0, 0 2, 0 0))"},
                         {"POINT (0.25 0.25)", "POINT (-1 -1)", std::nullopt}},
                        {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, std::nullopt}));
}

TEST(Predicates, SedonaUdfScalarScalar) {
  std::optional<std::string> lhs = "POLYGON ((0 0, 2 0, 0 2, 0 0))";
  std::string op = "intersects";
  std::optional<std::string> rhs = "POINT (0.25 0.25)";
  std::optional<bool> result = true;

  struct SedonaCScalarKernel kernel;
  struct SedonaCScalarKernelImpl impl;
  if (op == "intersects") {
    s2geography::sedona_udf::IntersectsKernel(&kernel);

  } else {
    ASSERT_TRUE(false) << "Unknown predicate";
  }

  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, {{lhs}, {rhs}},
                        {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL, {result}));
}
