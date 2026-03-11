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

struct ScalarScalarParam {
  std::string name;
  std::optional<std::string> lhs;
  std::string op;
  std::optional<std::string> rhs;
  std::optional<bool> expected;

  friend std::ostream& operator<<(std::ostream& os,
                                  const ScalarScalarParam& p) {
    os << (p.lhs ? *p.lhs : "null") << " " << p.op << " "
       << (p.rhs ? *p.rhs : "null") << " -> ";
    if (p.expected) {
      os << (*p.expected ? "true" : "false");
    } else {
      os << "null";
    }
    return os;
  }
};

class PredicatesScalarScalarTest
    : public ::testing::TestWithParam<ScalarScalarParam> {};

TEST_P(PredicatesScalarScalarTest, SedonaUdf) {
  const auto& p = GetParam();

  struct SedonaCScalarKernel kernel;
  struct SedonaCScalarKernelImpl impl;
  if (p.op == "intersects") {
    s2geography::sedona_udf::IntersectsKernel(&kernel);
  } else if (p.op == "contains") {
    s2geography::sedona_udf::ContainsKernel(&kernel);
  } else if (p.op == "equals") {
    s2geography::sedona_udf::EqualsKernel(&kernel);
  } else {
    FAIL() << "Unknown predicate: " << p.op;
  }

  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                        {{p.lhs}, {p.rhs}}, {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL, {p.expected}));
}

INSTANTIATE_TEST_SUITE_P(
    Predicates, PredicatesScalarScalarTest,
    ::testing::Values(
        // Intersects
        // Nulls
        ScalarScalarParam{"null_intersects", std::nullopt, "intersects",
                          "POINT EMPTY", std::nullopt},
        ScalarScalarParam{"intersects_null", "POINT EMPTY", "intersects",
                          std::nullopt, std::nullopt},
        ScalarScalarParam{"null_intersects_null", std::nullopt, "intersects",
                          std::nullopt, std::nullopt},

        // Intersects cases that take one of the faster paths
        // Point x polygon
        ScalarScalarParam{"polygon_intersects_point",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "intersects",
                          "POINT (0.25 0.25)", true},
        // Polygon x point
        ScalarScalarParam{"point_intersects_polygon", "POINT (0.25 0.25)",
                          "intersects", "POLYGON ((0 0, 2 0, 0 2, 0 0))", true},
        // Point definitely not in polygon (outside the covering)
        ScalarScalarParam{"polygon_not_intersects_distant_point",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "intersects",
                          "POINT (-30 -30)", false},
        // Point definitely not in polygon (probably inside the covering)
        ScalarScalarParam{"polygon_not_intersects_close_point",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "intersects",
                          "POINT (1.01 1.01)", false},

        // Polygon x boundary point
        ScalarScalarParam{"boundary_point_intersects_polygon", "POINT (0 0)",
                          "intersects", "POLYGON ((0 0, 2 0, 0 2, 0 0))", true},
        // Boundary point x polygon
        ScalarScalarParam{"polygon_intersects_boundary_point", "POINT (0 0)",
                          "intersects", "POLYGON ((0 0, 2 0, 0 2, 0 0))", true},

        // Polygon in polygon
        ScalarScalarParam{"polygon_intersects_polygon",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "intersects",
                          "POLYGON ((0 0, 1 0, 0 1, 0 0))", true},

        // Other predicates (currently there are no special cases here)
        ScalarScalarParam{"polygon_contains_point",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "contains",
                          "POINT (0.25 0.25)", true},
        ScalarScalarParam{"polygon_not_contains_point",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", "contains",
                          "POINT (-1 -1)", false},
        ScalarScalarParam{"polygon_equals_polygon",
                          "POLYGON ((0 0, 1 0, 0 1, 0 0))", "equals",
                          "POLYGON ((1 0, 0 1, 0 0, 1 0))", true},
        ScalarScalarParam{"polygon_not_equals_polygon",
                          "POLYGON ((0 0, 1 0, 0 1, 0 0))", "equals",
                          "POLYGON ((0 0, 2 0, 0 2, 0 0))", false}

        ),
    [](const ::testing::TestParamInfo<ScalarScalarParam>& info) {
      return info.param.name;
    });
