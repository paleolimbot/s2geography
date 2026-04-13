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
  std::optional<double> expected_max_distance;
  std::optional<std::string> expected_shortest_line;
  std::optional<std::string> expected_longest_line;
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
        SCOPED_TRACE("ST_Distance()");
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
        SCOPED_TRACE("ST_ShortestLine()");
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
        SCOPED_TRACE("ST_ClosestPoint()");
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

      // Test ST_MaxDistance
      {
        SCOPED_TRACE("ST_MaxDistance()");
        struct SedonaCScalarKernel kernel;
        struct SedonaCScalarKernelImpl impl;
        s2geography::sedona_udf::MaxDistanceKernel(&kernel, prepare_arg0,
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
            out_array.get(), NANOARROW_TYPE_DOUBLE, {p.expected_max_distance}));
      }

      // Test ST_LongestLine
      {
        SCOPED_TRACE("ST_LongestLine()");

        struct SedonaCScalarKernel kernel;
        struct SedonaCScalarKernelImpl impl;
        s2geography::sedona_udf::LongestLineKernel(&kernel, prepare_arg0,
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
            TestResultGeography(out_array.get(), {p.expected_longest_line}));
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    Distance, DistanceScalarScalarTest,
    ::testing::Values(
        // Nulls
        DistanceScalarScalarParam{"null_distance", std::nullopt, "POINT EMPTY",
                                  std::nullopt, std::nullopt, std::nullopt,
                                  std::nullopt, std::nullopt},
        DistanceScalarScalarParam{"distance_null", "POINT EMPTY", std::nullopt,
                                  std::nullopt, std::nullopt, std::nullopt,
                                  std::nullopt, std::nullopt},
        DistanceScalarScalarParam{"null_distance_null", std::nullopt,
                                  std::nullopt, std::nullopt, std::nullopt,
                                  std::nullopt, std::nullopt, std::nullopt},

        // Empties
        DistanceScalarScalarParam{"distance_empty", "POINT (0 0)",
                                  "POINT EMPTY", std::nullopt, std::nullopt,
                                  "LINESTRING EMPTY", "LINESTRING EMPTY",
                                  "POINT EMPTY"},
        DistanceScalarScalarParam{"empty_distance", "POINT EMPTY",
                                  "POINT (0 0)", std::nullopt, std::nullopt,
                                  "LINESTRING EMPTY", "LINESTRING EMPTY",
                                  "POINT EMPTY"},
        DistanceScalarScalarParam{"distance_empty_zm", "POINT ZM (0 0 0 0)",
                                  "POINT ZM EMPTY", std::nullopt, std::nullopt,
                                  "LINESTRING ZM EMPTY", "LINESTRING ZM EMPTY",
                                  "POINT ZM EMPTY"},
        DistanceScalarScalarParam{"empty_distance_zm", "POINT ZM EMPTY",
                                  "POINT ZM (0 0 0 0)", std::nullopt,
                                  std::nullopt, "LINESTRING ZM EMPTY",
                                  "LINESTRING ZM EMPTY", "POINT ZM EMPTY"},

        DistanceScalarScalarParam{
            // Point x point
            "point_distance_same_point", "POINT (0 0)", "POINT (0 0)",
            // Distance
            0.0,
            // Max distance
            0.0,
            // Shortest line
            "LINESTRING (0 0, 0 0)",
            // Longest line
            "LINESTRING (0 0, 0 0)",
            // Closest Point
            "POINT (0 0)"},
        DistanceScalarScalarParam{
            // Point x point
            "point_distance_point", "POINT (0 0)", "POINT (0 1)",
            // Distance
            111195.10117748393,
            // Max distance
            111195.10117748393,
            // Shortest line
            "LINESTRING (0 0, 0 1)",
            // Longest line
            "LINESTRING (0 0, 0 1)",
            // Closest Point
            "POINT (0 0)"},

        DistanceScalarScalarParam{
            // Point ZM x point ZM -------------------------
            "point_distance_point_zm", "POINT ZM (0 0 1 2)",
            "POINT ZM (0 1 2 3)",
            // Distance
            111195.10117748393,
            // Max distance
            111195.10117748393,
            // Shortest line
            "LINESTRING ZM (0 0 1 2, 0 1 2 3)",
            // Longest line
            "LINESTRING ZM (0 0 1 2, 0 1 2 3)",
            // Closest Point
            "POINT ZM (0 0 1 2)"},

        DistanceScalarScalarParam{
            // Point Z x point Z
            "point_distance_point_z", "POINT Z (0 0 1)", "POINT Z (0 1 2)",
            // Distance
            111195.10117748393,
            // Max distance
            111195.10117748393,
            // Shortest line
            "LINESTRING Z (0 0 1, 0 1 2)",
            // Longest line
            "LINESTRING Z (0 0 1, 0 1 2)",
            // Closest Point
            "POINT Z (0 0 1)"},

        DistanceScalarScalarParam{
            // Point M x point M
            "point_distance_point_m", "POINT M (0 0 2)", "POINT M (0 1 3)",
            // Distance
            111195.10117748393,
            // Max distance
            111195.10117748393,
            // Shortest line
            "LINESTRING M (0 0 2, 0 1 3)",
            // Longest line
            "LINESTRING M (0 0 2, 0 1 3)",
            // Closest Point
            "POINT M (0 0 2)"},

        DistanceScalarScalarParam{
            // Point x linestring (point on linestring) ----------
            "point_distance_linestring_on", "POINT (0 0)",
            "LINESTRING (0 0, 0 1)",
            // Distance
            0.0,
            // Max distance
            111195.10117748393,
            // Shortest line
            "LINESTRING (0 0, 0 0)",
            // Longest line
            "LINESTRING (0 0, 0 1)",
            // Closest Point
            "POINT (0 0)"},
        DistanceScalarScalarParam{
            // Point x linestring (point off linestring) ----------
            "point_distance_linestring_off", "POINT (1 0)",
            "LINESTRING (0 0, 0 1)",
            // Distance
            111195.10117748393,
            // Max distance
            157249.62809250789,
            // Shortest line
            "LINESTRING (1 0, 0 0)",
            // Longest line
            "LINESTRING (1 0, 0 1)",
            // Closest Point
            "POINT (1 0)"},

        DistanceScalarScalarParam{
            // Point x polygon (point inside)
            "point_distance_polygon_inside", "POINT (0.25 0.25)",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))",
            // Distance
            0.0,
            // Max distance
            196566.41390163341,
            // Shortest line
            "LINESTRING (0.25 0.25, 0.25 0.25)",
            // Longest line
            "LINESTRING (0.25 0.25, 2 0)",
            // Closest Point
            "POINT (0.25 0.25)"},
        DistanceScalarScalarParam{
            // Point x polygon (point on boundary)
            "point_distance_polygon_boundary", "POINT (0 0)",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))",
            // Distance
            0.0,
            // Max distance
            222390.20235496786,
            // Shortest line
            "LINESTRING (0 0, 0 0)",
            // Longest line
            "LINESTRING (0 0, 2 0)",
            // Closest Point
            "POINT (0 0)"},
        DistanceScalarScalarParam{
            // Point x polygon (point outside)
            "point_distance_polygon_outside", "POINT (-1 0)",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))",
            // Distance
            111195.10117748393,
            // Max distance
            333585.3035324518,
            // Shortest line
            "LINESTRING (-1 0, 0 0)",
            // Longest line
            "LINESTRING (-1 0, 2 0)",
            // Closest Point
            "POINT (-1 0)"},

        DistanceScalarScalarParam{
            // Z Point x polygon (point inside)
            "point_z_distance_polygon_inside", "POINT Z (0.25 0.25 10)",
            "POLYGON Z ((0 0 12, 2 0 12, 0 2 12, 0 0 12))",
            // Distance
            0.0,
            // Max distance
            196566.41390163341,
            // Shortest line
            "LINESTRING Z (0.25 0.25 10, 0.25 0.25 10)",
            // Longest line
            "LINESTRING Z (0.25 0.25 10, 2 0 12)",
            // Closest Point
            "POINT Z (0.25 0.25 10)"},
        DistanceScalarScalarParam{
            // Z Point x polygon (point on boundary)
            "point_z_distance_polygon_boundary", "POINT Z (0 0 10)",
            "POLYGON Z ((0 0 12, 2 0 12, 0 2 12, 0 0 12))",
            // Distance
            0.0,
            // Max distance
            222390.20235496786,
            // Shortest line
            "LINESTRING Z (0 0 10, 0 0 12)",
            // Longest line
            "LINESTRING Z (0 0 10, 2 0 12)",
            // Closest Point
            "POINT Z (0 0 10)"},
        DistanceScalarScalarParam{
            // Z Point x polygon (point outside)
            "point_z_distance_polygon_outside", "POINT Z (-1 0 10)",
            "POLYGON Z ((0 0 12, 2 0 12, 0 2 12, 0 0 12))",
            // Distance
            111195.10117748393,
            // Max distance
            333585.3035324518,
            // Shortest line
            "LINESTRING Z (-1 0 10, 0 0 12)",
            // Longest line
            "LINESTRING Z (-1 0 10, 2 0 12)",
            // Closest Point
            "POINT Z (-1 0 10)"},

        DistanceScalarScalarParam{
            // Linestring x polygon (linestring fully inside)
            "linestring_distance_polygon_inside",
            "LINESTRING (0.25 0.25, 0.5 0.5)", "POLYGON ((0 0, 2 0, 0 2, 0 0))",
            // Distance
            0.0,
            // Max distance
            196566.41390163341,
            // Shortest line
            "LINESTRING (0.25 0.25, 0.25 0.25)",
            // Longest line
            "LINESTRING (0.25 0.25, 2 0)",
            // Closest Point
            "POINT (0.25 0.25)"},
        DistanceScalarScalarParam{
            // Polygon x linestring (linestring fully inside)
            "polygon_distance_linestring_inside",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))", "LINESTRING (0.25 0.25, 0.5 0.5)",
            // Distance
            0.0,
            // Max distance
            196566.41390163341,
            // Shortest line
            "LINESTRING (0.25 0.25, 0.25 0.25)",
            // Longest line
            "LINESTRING (2 0, 0.25 0.25)",
            // Closest Point
            "POINT (0.25 0.25)"},

        DistanceScalarScalarParam{
            // Linestring x polygon (linestring partially crosses boundary)
            "linestring_distance_polygon_crossing",
            "LINESTRING (0.25 0.25, 3 3)", "POLYGON ((0 0, 2 0, 0 2, 0 0))",
            // Distance
            0.0,
            // Max distance
            471653.02881023812,
            // Shortest line
            "LINESTRING (0.999743 1.000714, 0.999743 1.000714)",
            // Longest line
            "LINESTRING (3 3, 0 0)",
            // Closest Point
            "POINT (0.999743 1.000714)"},
        DistanceScalarScalarParam{
            // Polygon x linestring (linestring partially crosses boundary)
            "polygon_distance_linestring_crossing",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))", "LINESTRING (0.25 0.25, 3 3)",
            // Distance
            0.0,
            // Max distance
            471653.02881023812,
            // Shortest line
            "LINESTRING (0.999743 1.000714, 0.999743 1.000714)",
            // Longest line
            "LINESTRING (0 0, 3 3)",
            // Closest Point
            "POINT (0.999743 1.000714)"},

        DistanceScalarScalarParam{
            // Linestring x polygon (linestring crosses through, neither vertex
            // inside)
            "linestring_distance_polygon_through", "LINESTRING (-1 0.5, 3 0.5)",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))",
            // Distance
            0.0,
            // Max distance
            372880.15844616242,
            // Shortest line
            "LINESTRING (1.5 0.500286, 1.5 0.500286)",
            // Longest line
            "LINESTRING (3 0.5, 0 2)",
            // Closest Point
            "POINT (1.5 0.500286)"},
        DistanceScalarScalarParam{
            // Polygon x linestring (linestring crosses through, neither vertex
            // inside)
            "polygon_distance_linestring_through",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))", "LINESTRING (-1 0.5, 3 0.5)",
            // Distance
            0.0,
            // Max distance
            372880.15844616242,
            // Shortest line
            "LINESTRING (1.5 0.500286, 1.5 0.500286)",
            // Longest line
            "LINESTRING (0 2, 3 0.5)",
            // Closest Point
            "POINT (1.5 0.500286)"},

        DistanceScalarScalarParam{
            // Linestring x polygon (linestring fully outside)
            "linestring_distance_polygon_outside", "LINESTRING (3 3, 4 4)",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))",
            // Distance
            314367.35908786184,
            // Max distance
            628758.78426786896,
            // Shortest line
            "LINESTRING (3 3, 0.998247 1.00221)",
            // Longest line
            "LINESTRING (4 4, 0 0)",
            // Closest Point
            "POINT (3 3)"},
        DistanceScalarScalarParam{
            // Polygon x linestring (linestring fully outside)
            "polygon_distance_linestring_outside",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))", "LINESTRING (3 3, 4 4)",
            // Distance
            314367.35908786184,
            // Max distance
            628758.78426786896,
            // Shortest line
            "LINESTRING (0.998247 1.00221, 3 3)",
            // Longest line
            "LINESTRING (0 0, 4 4)",
            // Closest Point
            "POINT (0.998247 1.00221)"},

        DistanceScalarScalarParam{
            // Polygon x polygon (one fully inside the other)
            "polygon_distance_polygon_inside", "POLYGON ((0 0, 2 0, 0 2, 0 0))",
            "POLYGON ((0.1 0.1, 0.5 0.1, 0.1 0.5, 0.1 0.1))",
            // Distance
            0.0,
            // Max distance
            218461.11755505961,
            // Shortest line
            "LINESTRING (0.1 0.1, 0.1 0.1)",
            // Longest line
            "LINESTRING (2 0, 0.1 0.5)",
            // Closest Point
            "POINT (0.1 0.1)"},
        DistanceScalarScalarParam{
            // Polygon x polygon (one fully inside, reversed)
            "polygon_distance_polygon_inside_rev",
            "POLYGON ((0.1 0.1, 0.5 0.1, 0.1 0.5, 0.1 0.1))",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))",
            // Distance
            0.0,
            // Max distance
            218461.11755505961,
            // Shortest line
            "LINESTRING (0.1 0.1, 0.1 0.1)",
            // Longest line
            "LINESTRING (0.1 0.5, 2 0)",
            // Closest Point
            "POINT (0.1 0.1)"},

        DistanceScalarScalarParam{
            // Polygon x polygon (partially overlapping) ----------
            "polygon_distance_polygon_crossing",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))", "POLYGON ((1 0, 3 0, 1 2, 1 0))",
            // Distance
            0.0,
            // Max distance
            400863.2536725945,
            // Shortest line
            "LINESTRING (2 0, 2 0)",
            // Longest line
            "LINESTRING (0 2, 3 0)",
            // Closest Point
            "POINT (2 0)"},
        DistanceScalarScalarParam{
            // Polygon x polygon (partially overlapping, reversed)
            "polygon_distance_polygon_crossing_rev",
            "POLYGON ((1 0, 3 0, 1 2, 1 0))", "POLYGON ((0 0, 2 0, 0 2, 0 0))",
            // Distance
            0.0,
            // Max distance
            400863.2536725945,
            // Shortest line
            "LINESTRING (2 0, 2 0)",
            // Longest line
            "LINESTRING (3 0, 0 2)",
            // Closest Point
            "POINT (2 0)"},

        DistanceScalarScalarParam{
            // Polygon x polygon (fully outside) ----------
            "polygon_distance_polygon_outside",
            "POLYGON ((0 0, 1 0, 0 1, 0 0))",
            "POLYGON ((30 30, 31 30, 30 31, 30 30))",
            // Distance
            4520972.0955287321,
            // Max distance
            4677959.9936393471,
            // Shortest line
            "LINESTRING (0 1, 30 30)",
            // Longest line
            "LINESTRING (0 0, 31 30)",
            // Closest Point
            "POINT (0 1)"},
        DistanceScalarScalarParam{
            // Polygon x polygon (fully outside, reversed) ----------
            "polygon_distance_polygon_outside_rev",
            "POLYGON ((30 30, 31 30, 30 31, 30 30))",
            "POLYGON ((0 0, 1 0, 0 1, 0 0))",
            // Distance
            4520972.0955287321,
            // Max distance
            4677959.9936393471,
            // Shortest line
            "LINESTRING (30 30, 0 1)",
            // Longest line
            "LINESTRING (31 30, 0 0)",
            // Closest Point
            "POINT (30 30)"},

        DistanceScalarScalarParam{
            // Polygon x polygon (north pole vs south pole) ----------
            "polygon_distance_polygon_poles",
            "POLYGON ((-120 80, 0 80, 120 80, -120 80))",
            "POLYGON ((-120 -80, 0 -80, 120 -80, -120 -80))",
            // Distance
            17791216.188397426,
            // Max distance
            20015118.21194711,
            // Shortest line
            "LINESTRING (-120 80, -120 -80)",
            // Longest line
            "LINESTRING (-30 84.187176, 150 -84.187176)",
            // Closest Point
            "POINT (-120 80)"},
        DistanceScalarScalarParam{
            // Polygon x polygon (north pole vs south pole, reversed) ----------
            "polygon_distance_polygon_poles_rev",
            "POLYGON ((-120 -80, 0 -80, 120 -80, -120 -80))",
            "POLYGON ((-120 80, 0 80, 120 80, -120 80))",
            // Distance
            17791216.188397426,
            // Max distance
            20015118.21194711,
            // Shortest line
            "LINESTRING (-120 -80, -120 80)",
            // Longest line
            "LINESTRING (150 -84.187176, -30 84.187176)",
            // Closest Point
            "POINT (-120 -80)"},
        DistanceScalarScalarParam{
            // Linestring x linestring (antipodal crossing) ----------
            "linestring_distance_linestring_poles",
            "LINESTRING (-90 -80, 90 -80)", "LINESTRING (0 80, 180 80)",
            // Distance
            18446595.193179362,
            // Max distance
            20015118.022076216,
            // Shortest line
            "LINESTRING (-90 -80, 0 80)",
            // Longest line
            "LINESTRING (-90 -90, 90 90)",
            // Closest Point
            "POINT (-90 -80)"},
        DistanceScalarScalarParam{
            // Linestring x polygon (antipodal crossing) ----------
            "linestring_distance_polygon_poles", "LINESTRING (-90 -80, 90 -80)",
            "POLYGON ((-120 90, 0 90, 120 90, -120 90))",
            // Distance
            18903167.200172286,
            // Max distance
            20015118.21194711,
            // Shortest line
            "LINESTRING (-90 -80, -120 90)",
            // Longest line
            "LINESTRING (75.445756 -90, -104.554244 90)",
            // Closest Point
            "POINT (-90 -80)"},
        DistanceScalarScalarParam{
            // Polygon x linestring (antipodal crossing) ----------
            "polygon_distance_linestring_poles",
            "POLYGON ((-120 90, 0 90, 120 90, -120 90))",
            "LINESTRING (-90 -80, 90 -80)",
            // Distance
            18903167.200172286,
            // Max distance
            20015118.21194711,
            // Shortest line
            "LINESTRING (-120 90, -90 -80)",
            // Longest line
            "LINESTRING (-104.554244 90, 75.445756 -90)",
            // Closest Point
            "POINT (-120 90)"},
        DistanceScalarScalarParam{
            // Point x point (antipodal crossing) ----------
            "point_distance_point_poles", "POINT (0 -90)", "POINT (0 90)",
            // Distance
            20015118.21194711,
            // Max distance
            20015118.21194711,
            // Shortest line
            "LINESTRING (0 -90, 0 90)",
            // Longest line
            "LINESTRING (0 -90, 0 90)",
            // Closest Point
            "POINT (0 -90)"}

        ),
    [](const ::testing::TestParamInfo<DistanceScalarScalarParam>& info) {
      return info.param.name;
    });

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
