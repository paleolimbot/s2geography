#include "s2geography/linear-referencing.h"

#include <gtest/gtest.h>

#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/sedona_udf/sedona_udf_test_internal.h"

using namespace s2geography;

TEST(LinearReferencing, SedonaUdfLineLocatePointArray) {
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

TEST(LinearReferencing, SedonaUdfLineInterpolatePointArray) {
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

struct LineLocatePointParam {
  std::string name;
  std::optional<std::string> line;
  std::optional<std::string> point;
  std::optional<double> expected;

  friend std::ostream& operator<<(std::ostream& os,
                                  const LineLocatePointParam& p) {
    os << (p.line ? *p.line : "null") << " locate "
       << (p.point ? *p.point : "null") << " -> ";
    if (p.expected) {
      os << *p.expected;
    } else {
      os << "null";
    }
    return os;
  }
};

class LineLocatePointTest
    : public ::testing::TestWithParam<LineLocatePointParam> {};

TEST_P(LineLocatePointTest, SedonaUdf) {
  const auto& p = GetParam();

  struct SedonaCScalarKernel kernel;
  struct SedonaCScalarKernelImpl impl;
  s2geography::sedona_udf::LineLocatePointKernel(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                        {{p.line}, {p.point}}, {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array.get(), NANOARROW_TYPE_DOUBLE, {p.expected}));
}

INSTANTIATE_TEST_SUITE_P(
    LinearReferencing, LineLocatePointTest,
    ::testing::Values(
        // Nulls
        LineLocatePointParam{"null_line", std::nullopt, "POINT (0 0)",
                             std::nullopt},
        LineLocatePointParam{"null_point", "LINESTRING (0 0, 0 1)",
                             std::nullopt, std::nullopt},
        LineLocatePointParam{"null_both", std::nullopt, std::nullopt,
                             std::nullopt},

        // Endpoints and midpoints
        LineLocatePointParam{"start", "LINESTRING (0 0, 0 2)", "POINT (0 0)",
                             0.0},
        LineLocatePointParam{"end", "LINESTRING (0 0, 0 2)", "POINT (0 2)",
                             1.0},
        LineLocatePointParam{"midpoint", "LINESTRING (0 0, 0 2)", "POINT (0 1)",
                             0.5},

        // Multi-segment line
        LineLocatePointParam{"multi_seg_quarter", "LINESTRING (0 0, 0 1, 0 2)",
                             "POINT (0 0.5)", 0.25},
        LineLocatePointParam{"multi_seg_three_quarter",
                             "LINESTRING (0 0, 0 1, 0 2)", "POINT (0 1.5)",
                             0.75},

        // Point off the line (projects to nearest)
        LineLocatePointParam{"off_line_start", "LINESTRING (0 0, 0 2)",
                             "POINT (1 0)", 0.0},
        LineLocatePointParam{"off_line_mid", "LINESTRING (0 0, 0 2)",
                             "POINT (1 1)", 0.50007614855210425}),
    [](const ::testing::TestParamInfo<LineLocatePointParam>& info) {
      return info.param.name;
    });

struct LineInterpolatePointParam {
  std::string name;
  std::optional<std::string> line;
  std::optional<double> fraction;
  std::optional<std::string> expected;

  friend std::ostream& operator<<(std::ostream& os,
                                  const LineInterpolatePointParam& p) {
    os << (p.line ? *p.line : "null") << " interpolate ";
    if (p.fraction) {
      os << *p.fraction;
    } else {
      os << "null";
    }
    os << " -> " << (p.expected ? *p.expected : "null");
    return os;
  }
};

class LineInterpolatePointTest
    : public ::testing::TestWithParam<LineInterpolatePointParam> {};

TEST_P(LineInterpolatePointTest, SedonaUdf) {
  const auto& p = GetParam();

  struct SedonaCScalarKernel kernel;
  struct SedonaCScalarKernelImpl impl;
  s2geography::sedona_udf::LineInterpolatePointKernel(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
                        {{p.line}}, {{p.fraction}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(out_array.get(), {p.expected}));
}

INSTANTIATE_TEST_SUITE_P(
    LinearReferencing, LineInterpolatePointTest,
    ::testing::Values(
        // Nulls
        LineInterpolatePointParam{"null_line", std::nullopt, 0.5, std::nullopt},
        LineInterpolatePointParam{"null_fraction", "LINESTRING (0 0, 0 2)",
                                  std::nullopt, std::nullopt},
        LineInterpolatePointParam{"null_both", std::nullopt, std::nullopt,
                                  std::nullopt},

        // Endpoints and midpoints
        LineInterpolatePointParam{"start", "LINESTRING (0 0, 0 2)", 0.0,
                                  "POINT (0 0)"},
        LineInterpolatePointParam{"end", "LINESTRING (0 0, 0 2)", 1.0,
                                  "POINT (0 2)"},
        LineInterpolatePointParam{"midpoint", "LINESTRING (0 0, 0 2)", 0.5,
                                  "POINT (0 1)"},

        // Multi-segment line
        LineInterpolatePointParam{"multi_seg_quarter",
                                  "LINESTRING (0 0, 0 1, 0 2)", 0.25,
                                  "POINT (0 0.5)"},
        LineInterpolatePointParam{"multi_seg_three_quarter",
                                  "LINESTRING (0 0, 0 1, 0 2)", 0.75,
                                  "POINT (0 1.5)"},

        // Boundary fractions
        LineInterpolatePointParam{"fraction_zero", "LINESTRING (0 0, 0 2)", 0.0,
                                  "POINT (0 0)"},
        LineInterpolatePointParam{"fraction_one", "LINESTRING (0 0, 0 2)", 1.0,
                                  "POINT (0 2)"}),
    [](const ::testing::TestParamInfo<LineInterpolatePointParam>& info) {
      return info.param.name;
    });
