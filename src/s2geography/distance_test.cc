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
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
      {{"POINT (0 0)"}, {"POINT (0 1)", "LINESTRING (0 0, 0 1)", std::nullopt}},
      {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array.get(), NANOARROW_TYPE_DOUBLE,
                      {111195.10117748393, 0.0, std::nullopt}));
}

struct DistanceScalarScalarParam {
  std::string name;
  std::optional<std::string> lhs;
  std::optional<std::string> rhs;
  std::optional<double> expected;
  std::optional<std::string> expected_shortest_line;
  std::optional<std::string> expected_closest_point;

  friend std::ostream& operator<<(std::ostream& os,
                                  const DistanceScalarScalarParam& p) {
    os << (p.lhs ? *p.lhs : "null") << " distance " << (p.rhs ? *p.rhs : "null")
       << " -> ";
    if (p.expected) {
      os << *p.expected;
    } else {
      os << "null";
    }
    return os;
  }
};

class DistanceScalarScalarTest
    : public ::testing::TestWithParam<DistanceScalarScalarParam> {};

TEST_P(DistanceScalarScalarTest, SedonaUdf) {
  const auto& p = GetParam();

  for (bool prepare_arg0 : {true, false}) {
    for (bool prepare_arg1 : {true, false}) {
      SCOPED_TRACE("prepare_arg0: " + std::to_string(prepare_arg0) +
                   ", prepare_arg1: " + std::to_string(prepare_arg1));
      // Test ST_Distance()
      {
        struct SedonaCScalarKernel kernel;
        struct SedonaCScalarKernelImpl impl;
        s2geography::sedona_udf::DistanceKernel(&kernel, prepare_arg0,
                                                prepare_arg1);

        ASSERT_NO_FATAL_FAILURE(TestInitKernel(&kernel, &impl,
                                               {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                                               NANOARROW_TYPE_DOUBLE));

        nanoarrow::UniqueArray out_array;
        ASSERT_NO_FATAL_FAILURE(
            TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                              {{p.lhs}, {p.rhs}}, {}, out_array.get()));
        impl.release(&impl);
        kernel.release(&kernel);

        ASSERT_NO_FATAL_FAILURE(TestResultArrow(
            out_array.get(), NANOARROW_TYPE_DOUBLE, {p.expected}));
      }

      // Test ST_ShortestLine
      {
        struct SedonaCScalarKernel kernel;
        struct SedonaCScalarKernelImpl impl;
        s2geography::sedona_udf::ShortestLineKernel(&kernel, prepare_arg0,
                                                    prepare_arg1);

        ASSERT_NO_FATAL_FAILURE(TestInitKernel(
            &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

        nanoarrow::UniqueArray out_array;
        ASSERT_NO_FATAL_FAILURE(
            TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                              {{p.lhs}, {p.rhs}}, {}, out_array.get()));
        impl.release(&impl);
        kernel.release(&kernel);

        ASSERT_NO_FATAL_FAILURE(
            TestResultGeography(out_array.get(), {p.expected_shortest_line}));
      }

      // Test ST_ClosestPoint
      {
        struct SedonaCScalarKernel kernel;
        struct SedonaCScalarKernelImpl impl;
        s2geography::sedona_udf::ClosestPointKernel(&kernel);

        ASSERT_NO_FATAL_FAILURE(TestInitKernel(
            &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

        nanoarrow::UniqueArray out_array;
        ASSERT_NO_FATAL_FAILURE(
            TestExecuteKernel(&impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
                              {{p.lhs}, {p.rhs}}, {}, out_array.get()));
        impl.release(&impl);
        kernel.release(&kernel);

        ASSERT_NO_FATAL_FAILURE(
            TestResultGeography(out_array.get(), {p.expected_closest_point}));
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    Distance, DistanceScalarScalarTest,
    ::testing::Values(
        // Nulls
        DistanceScalarScalarParam{"null_distance", std::nullopt, "POINT EMPTY",
                                  std::nullopt, std::nullopt, std::nullopt},
        DistanceScalarScalarParam{"distance_null", "POINT EMPTY", std::nullopt,
                                  std::nullopt, std::nullopt, std::nullopt},
        DistanceScalarScalarParam{"null_distance_null", std::nullopt,
                                  std::nullopt, std::nullopt, std::nullopt,
                                  std::nullopt},

        // Empties
        DistanceScalarScalarParam{"distance_empty", "POINT (0 0)",
                                  "POINT EMPTY", std::nullopt,
                                  "LINESTRING EMPTY", "POINT EMPTY"},
        DistanceScalarScalarParam{"empty_distance", "POINT EMPTY",
                                  "POINT (0 0)", std::nullopt,
                                  "LINESTRING EMPTY", "POINT EMPTY"},
        DistanceScalarScalarParam{"distance_empty_zm", "POINT ZM (0 0 0 0)",
                                  "POINT ZM EMPTY", std::nullopt,
                                  "LINESTRING ZM EMPTY", "POINT ZM EMPTY"},
        DistanceScalarScalarParam{"empty_distance_zm", "POINT ZM EMPTY",
                                  "POINT ZM (0 0 0 0)", std::nullopt,
                                  "LINESTRING ZM EMPTY", "POINT ZM EMPTY"},

        // Point x point
        DistanceScalarScalarParam{"point_distance_same_point", "POINT (0 0)",
                                  "POINT (0 0)", 0.0, "LINESTRING (0 0, 0 0)",
                                  "POINT (0 0)"},
        DistanceScalarScalarParam{"point_distance_point", "POINT (0 0)",
                                  "POINT (0 1)", 111195.10117748393,
                                  "LINESTRING (0 0, 0 1)", "POINT (0 0)"},

        DistanceScalarScalarParam{
            "point_distance_point_zm", "POINT ZM (0 0 1 2)",
            "POINT ZM (0 1 2 3)", 111195.10117748393,
            "LINESTRING ZM (0 0 1 2, 0 1 2 3)", "POINT ZM (0 0 1 2)"},

        DistanceScalarScalarParam{"point_distance_point_z", "POINT Z (0 0 1)",
                                  "POINT Z (0 1 2)", 111195.10117748393,
                                  "LINESTRING Z (0 0 1, 0 1 2)",
                                  "POINT Z (0 0 1)"},

        DistanceScalarScalarParam{"point_distance_point_m", "POINT M (0 0 2)",
                                  "POINT M (0 1 3)", 111195.10117748393,
                                  "LINESTRING M (0 0 2, 0 1 3)",
                                  "POINT M (0 0 2)"},

        // Point x linestring (point on linestring)
        DistanceScalarScalarParam{"point_distance_linestring_on", "POINT (0 0)",
                                  "LINESTRING (0 0, 0 1)", 0.0,
                                  "LINESTRING (0 0, 0 0)", "POINT (0 0)"},
        // Point x linestring (point off linestring)
        DistanceScalarScalarParam{"point_distance_linestring_off",
                                  "POINT (1 0)", "LINESTRING (0 0, 0 1)",
                                  111195.10117748393, "LINESTRING (1 0, 0 0)",
                                  "POINT (1 0)"},

        // Point x polygon (point inside)
        DistanceScalarScalarParam{
            "point_distance_polygon_inside", "POINT (0.25 0.25)",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))", 0.0,
            "LINESTRING (0.25 0.25, 0.25 0.25)", "POINT (0.25 0.25)"},
        // Point x polygon (point on boundary)
        DistanceScalarScalarParam{"point_distance_polygon_boundary",
                                  "POINT (0 0)",
                                  "POLYGON ((0 0, 2 0, 0 2, 0 0))", 0.0,
                                  "LINESTRING (0 0, 0 0)", "POINT (0 0)"},
        // Point x polygon (point outside)
        DistanceScalarScalarParam{
            "point_distance_polygon_outside", "POINT (-1 0)",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))", 111195.10117748393,
            "LINESTRING (-1 0, 0 0)", "POINT (-1 0)"},

        // Z Point x polygon (point inside)
        DistanceScalarScalarParam{
            "point_z_distance_polygon_inside", "POINT Z (0.25 0.25 10)",
            "POLYGON Z ((0 0 12, 2 0 12, 0 2 12, 0 0 12))", 0.0,
            "LINESTRING Z (0.25 0.25 10, 0.25 0.25 10)",
            "POINT Z (0.25 0.25 10)"},
        // Z Point x polygon (point on boundary)
        DistanceScalarScalarParam{
            "point_z_distance_polygon_boundary", "POINT Z (0 0 10)",
            "POLYGON Z ((0 0 12, 2 0 12, 0 2 12, 0 0 12))", 0.0,
            "LINESTRING Z (0 0 10, 0 0 12)", "POINT Z (0 0 10)"},
        // Z Point x polygon (point outside)
        DistanceScalarScalarParam{
            "point_z_distance_polygon_outside", "POINT Z (-1 0 10)",
            "POLYGON Z ((0 0 12, 2 0 12, 0 2 12, 0 0 12))", 111195.10117748393,
            "LINESTRING Z (-1 0 10, 0 0 12)", "POINT Z (-1 0 10)"},

        // Linestring x polygon (linestring fully inside)
        DistanceScalarScalarParam{
            "linestring_distance_polygon_inside",
            "LINESTRING (0.25 0.25, 0.5 0.5)", "POLYGON ((0 0, 2 0, 0 2, 0 0))",
            0.0, "LINESTRING (0.25 0.25, 0.25 0.25)", "POINT (0.25 0.25)"},
        // Polygon x linestring (linestring fully inside)
        DistanceScalarScalarParam{
            "polygon_distance_linestring_inside",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))", "LINESTRING (0.25 0.25, 0.5 0.5)",
            0.0, "LINESTRING (0.25 0.25, 0.25 0.25)", "POINT (0.25 0.25)"},

        // Linestring x polygon (linestring partially crosses boundary)
        DistanceScalarScalarParam{
            "linestring_distance_polygon_crossing",
            "LINESTRING (0.25 0.25, 3 3)", "POLYGON ((0 0, 2 0, 0 2, 0 0))",
            0.0, "LINESTRING (0.999743 1.000714, 0.999743 1.000714)",
            "POINT (0.999743 1.000714)"},
        // Polygon x linestring (linestring partially crosses boundary)
        DistanceScalarScalarParam{
            "polygon_distance_linestring_crossing",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))", "LINESTRING (0.25 0.25, 3 3)",
            0.0, "LINESTRING (0.999743 1.000714, 0.999743 1.000714)",
            "POINT (0.999743 1.000714)"},

        // Linestring x polygon (linestring crosses through, neither vertex
        // inside)
        DistanceScalarScalarParam{
            "linestring_distance_polygon_through", "LINESTRING (-1 0.5, 3 0.5)",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))", 0.0,
            "LINESTRING (1.5 0.500286, 1.5 0.500286)", "POINT (1.5 0.500286)"},
        // Polygon x linestring (linestring crosses through, neither vertex
        // inside)
        DistanceScalarScalarParam{
            "polygon_distance_linestring_through",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))", "LINESTRING (-1 0.5, 3 0.5)", 0.0,
            "LINESTRING (1.5 0.500286, 1.5 0.500286)", "POINT (1.5 0.500286)"},

        // Linestring x polygon (linestring fully outside)
        DistanceScalarScalarParam{
            "linestring_distance_polygon_outside", "LINESTRING (3 3, 4 4)",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))", 314367.35908786184,
            "LINESTRING (3 3, 0.998247 1.00221)", "POINT (3 3)"},
        // Polygon x linestring (linestring fully outside)
        DistanceScalarScalarParam{"polygon_distance_linestring_outside",
                                  "POLYGON ((0 0, 2 0, 0 2, 0 0))",
                                  "LINESTRING (3 3, 4 4)", 314367.35908786184,
                                  "LINESTRING (0.998247 1.00221, 3 3)",
                                  "POINT (0.998247 1.00221)"},

        // Polygon x polygon (one fully inside the other)
        DistanceScalarScalarParam{
            "polygon_distance_polygon_inside", "POLYGON ((0 0, 2 0, 0 2, 0 0))",
            "POLYGON ((0.1 0.1, 0.5 0.1, 0.1 0.5, 0.1 0.1))", 0.0,
            "LINESTRING (0.1 0.1, 0.1 0.1)", "POINT (0.1 0.1)"},
        // Polygon x polygon (one fully inside, reversed)
        DistanceScalarScalarParam{
            "polygon_distance_polygon_inside_rev",
            "POLYGON ((0.1 0.1, 0.5 0.1, 0.1 0.5, 0.1 0.1))",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))", 0.0,
            "LINESTRING (0.1 0.1, 0.1 0.1)", "POINT (0.1 0.1)"},

        // Polygon x polygon (partially overlapping)
        DistanceScalarScalarParam{"polygon_distance_polygon_crossing",
                                  "POLYGON ((0 0, 2 0, 0 2, 0 0))",
                                  "POLYGON ((1 0, 3 0, 1 2, 1 0))", 0.0,
                                  "LINESTRING (2 0, 2 0)", "POINT (2 0)"},
        // Polygon x polygon (partially overlapping, reversed)
        DistanceScalarScalarParam{"polygon_distance_polygon_crossing_rev",
                                  "POLYGON ((1 0, 3 0, 1 2, 1 0))",
                                  "POLYGON ((0 0, 2 0, 0 2, 0 0))", 0.0,
                                  "LINESTRING (2 0, 2 0)", "POINT (2 0)"},

        // Polygon x polygon (fully outside)
        DistanceScalarScalarParam{"polygon_distance_polygon_outside",
                                  "POLYGON ((0 0, 1 0, 0 1, 0 0))",
                                  "POLYGON ((30 30, 31 30, 30 31, 30 30))",
                                  4520972.0955287321, "LINESTRING (0 1, 30 30)",
                                  "POINT (0 1)"},
        // Polygon x polygon (fully outside, reversed)
        DistanceScalarScalarParam{"polygon_distance_polygon_outside_rev",
                                  "POLYGON ((30 30, 31 30, 30 31, 30 30))",
                                  "POLYGON ((0 0, 1 0, 0 1, 0 0))",
                                  4520972.0955287321, "LINESTRING (30 30, 0 1)",
                                  "POINT (30 30)"}

        ),
    [](const ::testing::TestParamInfo<DistanceScalarScalarParam>& info) {
      return info.param.name;
    });

TEST(Distance, SedonaUdfMaxDistance) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::MaxDistanceKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
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
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
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
      &impl, {ARROW_TYPE_WKB, ARROW_TYPE_WKB},
      {{"POINT (0 1)", "LINESTRING (0 0, 0 1)", std::nullopt}, {"POINT (0 0)"}},
      {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 1)", "POINT (0 0)", std::nullopt}));
}
