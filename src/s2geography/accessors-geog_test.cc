#include "s2geography/accessors-geog.h"

#include <gtest/gtest.h>

#include "geoarrow/geoarrow.hpp"
#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/sedona_udf/sedona_udf_test_internal.h"

TEST(Accessors, SedonaUdfAreaArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::AreaKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(
      TestInitKernel(&kernel, &impl, {ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB},
                        {{"POINT (0 1)", "LINESTRING (0 0, 0 1)",
                          "POLYGON ((0 0, 0 1, 1 0, 0 0))", std::nullopt}},
                        {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array.get(), NANOARROW_TYPE_DOUBLE,
                      {0.0, 0.0, 6182489130.9071951, std::nullopt}));
}

TEST(Accessors, SedonaUdfLengthArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::LengthKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(
      TestInitKernel(&kernel, &impl, {ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB},
                        {{"POINT (0 1)", "LINESTRING (0 0, 0 1)",
                          "POLYGON ((0 0, 0 1, 1 0, 0 0))", std::nullopt}},
                        {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array.get(), NANOARROW_TYPE_DOUBLE,
                      {0.0, 111195.10117748393, 0.0, std::nullopt}));
}

TEST(AccessorsGeog, SedonaUdfCentroidArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::CentroidKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(
      TestInitKernel(&kernel, &impl, {ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB},
                        {{"POINT (0 1)", "LINESTRING (0 0, 0 1)",
                          "POLYGON ((0 0, 0 1, 1 0, 0 0))", std::nullopt}},
                        {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 1)", "POINT (0 0.5)",
                        "POINT (0.33335 0.333344)", std::nullopt}));
}

TEST(AccessorsGeog, SedonaUdfConvexHullArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::ConvexHullKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(
      TestInitKernel(&kernel, &impl, {ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB},
                        {{"POINT (0 1)", "LINESTRING (0 0, 0 1)",
                          "POLYGON ((0 0, 0 1, 1 0, 0 0))", std::nullopt}},
                        {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 1)", "LINESTRING (0 0, 0 1)",
                        "POLYGON ((0 0, 1 0, 0 1, 0 0))", std::nullopt}));
}

TEST(AccessorsGeog, SedonaUdfPointOnSurfaceArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::PointOnSurfaceKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(
      TestInitKernel(&kernel, &impl, {ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB},
                        {{"POINT (0 1)", "LINESTRING (0 0, 0 1)",
                          "POLYGON ((0 0, 0 1, 1 0, 0 0))", std::nullopt}},
                        {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 1)", "POINT (0 1)",
                        "POINT (0.224466 0.224464)", std::nullopt}));
}

struct UnaryDoubleScalarParam {
  std::string name;
  std::optional<std::string> input;
  std::optional<double> expected;

  friend std::ostream& operator<<(std::ostream& os,
                                  const UnaryDoubleScalarParam& p) {
    os << (p.input ? *p.input : "null") << " -> ";
    if (p.expected) {
      os << *p.expected;
    } else {
      os << "null";
    }
    return os;
  }
};

void TestUnaryScalarDouble(void (*init_kernel)(struct SedonaCScalarKernel*),
                           const UnaryDoubleScalarParam& p) {
  struct SedonaCScalarKernel kernel;
  init_kernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(
      TestInitKernel(&kernel, &impl, {ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(&impl, {ARROW_TYPE_WKB},
                                            {{p.input}}, {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array.get(), NANOARROW_TYPE_DOUBLE, {p.expected}));
}

class AreaScalarTest : public ::testing::TestWithParam<UnaryDoubleScalarParam> {
};

TEST_P(AreaScalarTest, SedonaUdf) {
  ASSERT_NO_FATAL_FAILURE(
      TestUnaryScalarDouble(s2geography::sedona_udf::AreaKernel, GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    Accessors, AreaScalarTest,
    ::testing::Values(
        // Nulls
        UnaryDoubleScalarParam{"null_area", std::nullopt, std::nullopt},

        // Empties
        UnaryDoubleScalarParam{"point_empty", "POINT EMPTY", 0.0},
        UnaryDoubleScalarParam{"linestring_empty", "LINESTRING EMPTY", 0.0},
        UnaryDoubleScalarParam{"polygon_empty", "POLYGON EMPTY", 0.0},

        // Points (zero area)
        UnaryDoubleScalarParam{"point", "POINT (0 0)", 0.0},
        UnaryDoubleScalarParam{"multipoint", "MULTIPOINT ((0 0), (1 1))", 0.0},

        // Linestrings (zero area)
        UnaryDoubleScalarParam{"linestring", "LINESTRING (0 0, 0 1)", 0.0},
        UnaryDoubleScalarParam{"multilinestring",
                               "MULTILINESTRING ((0 0, 0 1), (1 0, 1 1))", 0.0},

        // Polygons
        UnaryDoubleScalarParam{"triangle", "POLYGON ((0 0, 0 1, 1 0, 0 0))",
                               6182489130.9071951},
        UnaryDoubleScalarParam{"square", "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))",
                               12364036567.076418},

        // Multipolygon
        UnaryDoubleScalarParam{
            "multipolygon",
            "MULTIPOLYGON (((0 0, 0 1, 1 0, 0 0)), ((10 10, 10 11, "
            "11 10, 10 10)))",
            12271037686.230379},

        // Polygon with hole
        UnaryDoubleScalarParam{
            "polygon_with_hole",
            "POLYGON ((0 0, 0 2, 2 0, 0 0), (0.1 0.1, 0.1 0.5, 0.5 "
            "0.1, 0.1 0.1))",
            23744568445.094166}

        ),
    [](const ::testing::TestParamInfo<UnaryDoubleScalarParam>& info) {
      return info.param.name;
    });

class LengthScalarTest
    : public ::testing::TestWithParam<UnaryDoubleScalarParam> {};

TEST_P(LengthScalarTest, SedonaUdf) {
  ASSERT_NO_FATAL_FAILURE(
      TestUnaryScalarDouble(s2geography::sedona_udf::LengthKernel, GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    Accessors, LengthScalarTest,
    ::testing::Values(
        // Nulls
        UnaryDoubleScalarParam{"null_length", std::nullopt, std::nullopt},

        // Empties
        UnaryDoubleScalarParam{"point_empty", "POINT EMPTY", 0.0},
        UnaryDoubleScalarParam{"linestring_empty", "LINESTRING EMPTY", 0.0},
        UnaryDoubleScalarParam{"polygon_empty", "POLYGON EMPTY", 0.0},

        // Points (zero length)
        UnaryDoubleScalarParam{"point", "POINT (0 0)", 0.0},
        UnaryDoubleScalarParam{"multipoint", "MULTIPOINT ((0 0), (1 1))", 0.0},

        // Linestrings
        UnaryDoubleScalarParam{"linestring_one_segment",
                               "LINESTRING (0 0, 0 1)", 111195.10117748393},
        UnaryDoubleScalarParam{"linestring_two_segments",
                               "LINESTRING (0 0, 0 1, 1 1)",
                               222373.26637265272},
        UnaryDoubleScalarParam{"multilinestring",
                               "MULTILINESTRING ((0 0, 0 1), (1 0, 1 1))",
                               222390.20235496786},

        // Polygons (zero length — perimeter is separate)
        UnaryDoubleScalarParam{"triangle", "POLYGON ((0 0, 0 1, 1 0, 0 0))",
                               0.0},
        UnaryDoubleScalarParam{"square", "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))",
                               0.0}

        ),
    [](const ::testing::TestParamInfo<UnaryDoubleScalarParam>& info) {
      return info.param.name;
    });

class PerimeterScalarTest
    : public ::testing::TestWithParam<UnaryDoubleScalarParam> {};

TEST_P(PerimeterScalarTest, SedonaUdf) {
  ASSERT_NO_FATAL_FAILURE(TestUnaryScalarDouble(
      s2geography::sedona_udf::PerimeterKernel, GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    Accessors, PerimeterScalarTest,
    ::testing::Values(
        // Nulls
        UnaryDoubleScalarParam{"null_perimeter", std::nullopt, std::nullopt},

        // Empties
        UnaryDoubleScalarParam{"point_empty", "POINT EMPTY", 0.0},
        UnaryDoubleScalarParam{"linestring_empty", "LINESTRING EMPTY", 0.0},
        UnaryDoubleScalarParam{"polygon_empty", "POLYGON EMPTY", 0.0},

        // Points (zero perimeter)
        UnaryDoubleScalarParam{"point", "POINT (0 0)", 0.0},

        // Linestrings (zero perimeter)
        UnaryDoubleScalarParam{"linestring", "LINESTRING (0 0, 0 1)", 0.0},

        // Polygons
        UnaryDoubleScalarParam{"triangle", "POLYGON ((0 0, 0 1, 1 0, 0 0))",
                               379639.83044747578},
        UnaryDoubleScalarParam{"square", "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))",
                               444763.46872762055},

        // Multipolygon
        UnaryDoubleScalarParam{
            "multipolygon",
            "MULTIPOLYGON (((0 0, 0 1, 1 0, 0 0)), ((10 10, 10 11, 11 10, 10 "
            "10)))",
            756282.14701838186},

        // Polygon with hole
        UnaryDoubleScalarParam{
            "polygon_with_hole",
            "POLYGON ((0 0, 0 2, 2 0, 0 0), (0.1 0.1, 0.1 0.5, 0.5 0.1, 0.1 "
            "0.1))",
            911112.66968130425}

        ),
    [](const ::testing::TestParamInfo<UnaryDoubleScalarParam>& info) {
      return info.param.name;
    });

struct UnaryGeographyScalarParam {
  std::string name;
  std::optional<std::string> input;
  std::optional<std::string> expected;

  friend std::ostream& operator<<(std::ostream& os,
                                  const UnaryGeographyScalarParam& p) {
    os << (p.input ? *p.input : "null") << " -> ";
    if (p.expected) {
      os << *p.expected;
    } else {
      os << "null";
    }
    return os;
  }
};

void TestUnaryScalarGeography(void (*init_kernel)(struct SedonaCScalarKernel*),
                              const UnaryGeographyScalarParam& p) {
  struct SedonaCScalarKernel kernel;
  init_kernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(
      TestInitKernel(&kernel, &impl, {ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(&impl, {ARROW_TYPE_WKB},
                                            {{p.input}}, {}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(out_array.get(), {p.expected}));
}

class CentroidScalarTest
    : public ::testing::TestWithParam<UnaryGeographyScalarParam> {};

TEST_P(CentroidScalarTest, SedonaUdf) {
  ASSERT_NO_FATAL_FAILURE(TestUnaryScalarGeography(
      s2geography::sedona_udf::CentroidKernel, GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    AccessorsGeog, CentroidScalarTest,
    ::testing::Values(
        // Nulls
        UnaryGeographyScalarParam{"null_centroid", std::nullopt, std::nullopt},

        // Empties
        UnaryGeographyScalarParam{"point_empty", "POINT EMPTY",
                                  "GEOMETRYCOLLECTION EMPTY"},
        UnaryGeographyScalarParam{"linestring_empty", "LINESTRING EMPTY",
                                  "GEOMETRYCOLLECTION EMPTY"},
        UnaryGeographyScalarParam{"polygon_empty", "POLYGON EMPTY",
                                  "GEOMETRYCOLLECTION EMPTY"},

        // Points
        UnaryGeographyScalarParam{"point", "POINT (0 1)", "POINT (0 1)"},
        UnaryGeographyScalarParam{"multipoint", "MULTIPOINT ((0 0), (0 1))",
                                  "POINT (0 0.5)"},
        UnaryGeographyScalarParam{"point_z", "POINT Z (0 1 10)",
                                  "POINT Z (0 1 10)"},
        UnaryGeographyScalarParam{"multipoint_z",
                                  "MULTIPOINT Z ((0 0 10), (0 1 11))",
                                  "POINT Z (0 0.5 10.5)"},
        UnaryGeographyScalarParam{"point_m", "POINT M (0 1 10)",
                                  "POINT M (0 1 10)"},
        UnaryGeographyScalarParam{"multipoint_m",
                                  "MULTIPOINT M ((0 0 10), (0 1 11))",
                                  "POINT M (0 0.5 10.5)"},
        UnaryGeographyScalarParam{"point_zm", "POINT ZM (0 1 10 20)",
                                  "POINT ZM (0 1 10 20)"},
        UnaryGeographyScalarParam{"multipoint_zm",
                                  "MULTIPOINT ZM ((0 0 10 20), (0 1 11 21))",
                                  "POINT ZM (0 0.5 10.5 20.5)"},

        // Linestrings
        UnaryGeographyScalarParam{"linestring", "LINESTRING (0 0, 0 1)",
                                  "POINT (0 0.5)"},
        UnaryGeographyScalarParam{"linestring_two_segments",
                                  "LINESTRING (0 0, 0 1, 0 5)",
                                  "POINT (0 2.5)"},
        UnaryGeographyScalarParam{"multilinestring",
                                  "MULTILINESTRING ((0 0, 0 1), (10 0, 10 5))",
                                  "POINT (8.336347 2.171205)"},
        UnaryGeographyScalarParam{"linestring_z",
                                  "LINESTRING Z (0 0 10, 0 1 11)",
                                  "POINT Z (0 0.5 10.5)"},
        UnaryGeographyScalarParam{"linestring_two_segments_z",
                                  "LINESTRING Z (0 0 10, 0 1 11, 0 5 15)",
                                  "POINT Z (0 2.5 12.5)"},
        UnaryGeographyScalarParam{"linestring_m",
                                  "LINESTRING M (0 0 10, 0 1 11)",
                                  "POINT M (0 0.5 10.5)"},
        UnaryGeographyScalarParam{"linestring_two_segments_m",
                                  "LINESTRING M (0 0 10, 0 1 11, 0 5 15)",
                                  "POINT M (0 2.5 12.5)"},
        UnaryGeographyScalarParam{"linestring_zm",
                                  "LINESTRING ZM (0 0 10 20, 0 1 11 21)",
                                  "POINT ZM (0 0.5 10.5 20.5)"},
        UnaryGeographyScalarParam{
            "linestring_two_segments_zm",
            "LINESTRING ZM (0 0 10 20, 0 1 11 21, 0 5 15 25)",
            "POINT ZM (0 2.5 12.5 22.5)"},

        // Polygons
        UnaryGeographyScalarParam{"triangle", "POLYGON ((0 0, 0 1, 1 0, 0 0))",
                                  "POINT (0.33335 0.333344)"},
        UnaryGeographyScalarParam{
            "polygon_with_hole",
            "POLYGON ((0 0, 0 2, 2 0, 0 0), (0.1 0.1, 0.1 0.5, 0.5 "
            "0.1, 0.1 0.1))",
            "POINT (0.684859 0.68481)"},
        UnaryGeographyScalarParam{
            "multipolygon",
            "MULTIPOLYGON (((0 0, 0 1, 1 0, 0 0)), ((10 10, 10 11, "
            "11 10, 10 10)))",
            "POINT (5.254205 5.315242)"},
        UnaryGeographyScalarParam{
            "multipolygon_with_hole",
            "MULTIPOLYGON (((0 0, 0 2, 2 0, 0 0), (0.1 0.1, 0.1 0.5, "
            "0.5 0.1, 0.1 0.1)), ((10 10, 10 11, 11 10, 10 10)))",
            "POINT (2.624356 2.655749)"},

        UnaryGeographyScalarParam{
            "triangle_z", "POLYGON Z ((0 0 10, 0 1 10, 1 0 10, 0 0 10))",
            "POINT Z (0.33335 0.333344 10)"},

        // Resulting Z should be between 10 and 11 but closer to 10
        UnaryGeographyScalarParam{"polygon_with_hole_z",
                                  "POLYGON Z ((0 0 10, 0 2 10, 2 0 10, 0 0 "
                                  "10), (0.1 0.1 11, 0.1 0.5 11, 0.5 "
                                  "0.1 11, 0.1 0.1 11))",
                                  "POINT Z (0.684859 0.68481 10.038454)"},

        UnaryGeographyScalarParam{
            "triangle_m", "POLYGON M ((0 0 10, 0 1 10, 1 0 10, 0 0 10))",
            "POINT M (0.33335 0.333344 10)"},

        // Resulting M should be between 10 and 11 but closer to 10
        UnaryGeographyScalarParam{"polygon_with_hole_m",
                                  "POLYGON M ((0 0 10, 0 2 10, 2 0 10, 0 0 "
                                  "10), (0.1 0.1 11, 0.1 0.5 11, 0.5 "
                                  "0.1 11, 0.1 0.1 11))",
                                  "POINT M (0.684859 0.68481 10.038454)"},

        UnaryGeographyScalarParam{
            "triangle_zm",
            "POLYGON ZM ((0 0 10 20, 0 1 10 20, 1 0 10 20, 0 0 10 20))",
            "POINT ZM (0.33335 0.333344 10 20)"},

        // Resulting Z and M should be between 10/20 and 11/21 but closer to
        // 10/20
        UnaryGeographyScalarParam{
            "polygon_with_hole_zm",
            "POLYGON ZM ((0 0 10 20, 0 2 10 20, 2 0 10 20, 0 0 "
            "10 20), (0.1 0.1 11 21, 0.1 0.5 11 21, 0.5 "
            "0.1 11 21, 0.1 0.1 11 21))",
            "POINT ZM (0.684859 0.68481 10.038454 20.038454)"}

        ),
    [](const ::testing::TestParamInfo<UnaryGeographyScalarParam>& info) {
      return info.param.name;
    });

class ConvexHullScalarTest
    : public ::testing::TestWithParam<UnaryGeographyScalarParam> {};

TEST_P(ConvexHullScalarTest, SedonaUdf) {
  ASSERT_NO_FATAL_FAILURE(TestUnaryScalarGeography(
      s2geography::sedona_udf::ConvexHullKernel, GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    AccessorsGeog, ConvexHullScalarTest,
    ::testing::Values(
        // Nulls
        UnaryGeographyScalarParam{"null_convex_hull", std::nullopt,
                                  std::nullopt},

        // Empties
        UnaryGeographyScalarParam{"point_empty", "POINT EMPTY",
                                  "GEOMETRYCOLLECTION EMPTY"},
        UnaryGeographyScalarParam{"linestring_empty", "LINESTRING EMPTY",
                                  "GEOMETRYCOLLECTION EMPTY"},
        UnaryGeographyScalarParam{"polygon_empty", "POLYGON EMPTY",
                                  "GEOMETRYCOLLECTION EMPTY"},

        // Points
        UnaryGeographyScalarParam{"point", "POINT (0 1)", "POINT (0 1)"},
        UnaryGeographyScalarParam{"multipoint_two", "MULTIPOINT ((0 0), (0 1))",
                                  "LINESTRING (0 0, 0 1)"},
        UnaryGeographyScalarParam{"multipoint_three",
                                  "MULTIPOINT ((0 0), (0 1), (1 0))",
                                  "POLYGON ((0 0, 1 0, 0 1, 0 0))"},

        // Linestrings
        UnaryGeographyScalarParam{"linestring", "LINESTRING (0 0, 0 1)",
                                  "LINESTRING (0 0, 0 1)"},
        UnaryGeographyScalarParam{"linestring_colinear",
                                  "LINESTRING (0 0, 0 1, 0 2)",
                                  "LINESTRING (0 0, 0 2)"},
        UnaryGeographyScalarParam{"linestring_non_colinear",
                                  "LINESTRING (0 0, 0 1, 1 0)",
                                  "POLYGON ((0 0, 1 0, 0 1, 0 0))"},

        // Polygons
        UnaryGeographyScalarParam{"triangle", "POLYGON ((0 0, 0 1, 1 0, 0 0))",
                                  "POLYGON ((0 0, 1 0, 0 1, 0 0))"},
        UnaryGeographyScalarParam{
            "polygon_with_hole",
            "POLYGON ((0 0, 0 2, 2 0, 0 0), (0.1 0.1, 0.1 0.5, 0.5 "
            "0.1, 0.1 0.1))",
            "POLYGON ((0 0, 2 0, 0 2, 0 0))"},
        UnaryGeographyScalarParam{
            "multipolygon_with_hole",
            "MULTIPOLYGON (((0 0, 0 2, 2 0, 0 0), (0.1 0.1, 0.1 0.5, "
            "0.5 0.1, 0.1 0.1)), ((10 10, 10 11, 11 10, 10 10)))",
            "POLYGON ((0 0, 2 0, 11 10, 10 11, 0 2, 0 0))"}

        ),
    [](const ::testing::TestParamInfo<UnaryGeographyScalarParam>& info) {
      return info.param.name;
    });

class PointOnSurfaceScalarTest
    : public ::testing::TestWithParam<UnaryGeographyScalarParam> {};

TEST_P(PointOnSurfaceScalarTest, SedonaUdf) {
  ASSERT_NO_FATAL_FAILURE(TestUnaryScalarGeography(
      s2geography::sedona_udf::PointOnSurfaceKernel, GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    AccessorsGeog, PointOnSurfaceScalarTest,
    ::testing::Values(
        // Nulls
        UnaryGeographyScalarParam{"null_point_on_surface", std::nullopt,
                                  std::nullopt},

        // Empties
        UnaryGeographyScalarParam{"point_empty", "POINT EMPTY", "POINT EMPTY"},
        UnaryGeographyScalarParam{"linestring_empty", "LINESTRING EMPTY",
                                  "POINT EMPTY"},
        UnaryGeographyScalarParam{"polygon_empty", "POLYGON EMPTY",
                                  "POINT EMPTY"},

        // Points
        UnaryGeographyScalarParam{"point", "POINT (0 1)", "POINT (0 1)"},
        UnaryGeographyScalarParam{"multipoint", "MULTIPOINT ((0 0), (0 1))",
                                  "POINT (0 1)"},

        // Linestrings
        UnaryGeographyScalarParam{"linestring", "LINESTRING (0 0, 0 1)",
                                  "POINT (0 1)"},
        UnaryGeographyScalarParam{"linestring_three_vertices",
                                  "LINESTRING (0 0, 0 1, 0 5)", "POINT (0 1)"},
        UnaryGeographyScalarParam{"multilinestring",
                                  "MULTILINESTRING ((0 0, 0 1), (10 0, 10 5))",
                                  "POINT (10 0)"},

        // Linestrings with Z/M
        UnaryGeographyScalarParam{"linestring_z",
                                  "LINESTRING Z (0 0 10, 0 1 11)",
                                  "POINT Z (0 1 11)"},
        UnaryGeographyScalarParam{"linestring_m",
                                  "LINESTRING M (0 0 10, 0 1 11)",
                                  "POINT M (0 1 11)"},
        UnaryGeographyScalarParam{"linestring_zm",
                                  "LINESTRING ZM (0 0 10 20, 0 1 11 21)",
                                  "POINT ZM (0 1 11 21)"},

        // Polygons
        UnaryGeographyScalarParam{"triangle", "POLYGON ((0 0, 0 1, 1 0, 0 0))",
                                  "POINT (0.224466 0.224464)"},
        UnaryGeographyScalarParam{"square",
                                  "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))",
                                  "POINT (0.450237 0.450223)"},
        UnaryGeographyScalarParam{
            "polygon_with_hole",
            "POLYGON ((0 0, 0 2, 2 0, 0 0), (0.1 0.1, 0.1 0.5, 0.5 "
            "0.1, 0.1 0.1))",
            "POINT (1.058613 0.45016)"},
        UnaryGeographyScalarParam{
            "multipolygon",
            "MULTIPOLYGON (((0 0, 0 1, 1 0, 0 0)), ((10 10, 10 11, "
            "11 10, 10 10)))",
            "POINT (0.224466 0.224464)"}

        ),
    [](const ::testing::TestParamInfo<UnaryGeographyScalarParam>& info) {
      return info.param.name;
    });
