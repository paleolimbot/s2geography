#include "s2geography/accessors-geog.h"

#include <gtest/gtest.h>

#include "geoarrow/geoarrow.hpp"
#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/sedona_udf/sedona_udf_test_internal.h"

TEST(Accessors, SedonaUdfArea) {
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

TEST(Accessors, SedonaUdfLength) {
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

TEST(AccessorsGeog, SedonaUdfCentroid) {
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

TEST(AccessorsGeog, SedonaUdfConvexHull) {
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


struct AreaScalarParam {
  std::string name;
  std::optional<std::string> input;
  std::optional<double> expected;

  friend std::ostream& operator<<(std::ostream& os, const AreaScalarParam& p) {
    os << (p.input ? *p.input : "null") << " -> ";
    if (p.expected) {
      os << *p.expected;
    } else {
      os << "null";
    }
    return os;
  }
};

class AreaScalarTest : public ::testing::TestWithParam<AreaScalarParam> {};

TEST_P(AreaScalarTest, SedonaUdf) {
  const auto& p = GetParam();

  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::AreaKernel(&kernel);
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

INSTANTIATE_TEST_SUITE_P(
    Accessors, AreaScalarTest,
    ::testing::Values(
        // Nulls
        AreaScalarParam{"null_area", std::nullopt, std::nullopt},

        // Empties
        AreaScalarParam{"point_empty", "POINT EMPTY", 0.0},
        AreaScalarParam{"linestring_empty", "LINESTRING EMPTY", 0.0},
        AreaScalarParam{"polygon_empty", "POLYGON EMPTY", 0.0},

        // Points (zero area)
        AreaScalarParam{"point", "POINT (0 0)", 0.0},
        AreaScalarParam{"multipoint", "MULTIPOINT ((0 0), (1 1))", 0.0},

        // Linestrings (zero area)
        AreaScalarParam{"linestring", "LINESTRING (0 0, 0 1)", 0.0},
        AreaScalarParam{"multilinestring",
                        "MULTILINESTRING ((0 0, 0 1), (1 0, 1 1))", 0.0},

        // Polygons
        AreaScalarParam{"triangle", "POLYGON ((0 0, 0 1, 1 0, 0 0))",
                        6182489130.9071951},
        AreaScalarParam{"square", "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))",
                        12364036567.076418},

        // Multipolygon
        AreaScalarParam{
            "multipolygon",
            "MULTIPOLYGON (((0 0, 0 1, 1 0, 0 0)), ((10 10, 10 11, 11 10, 10 10)))",
            12271037686.230379},

        // Polygon with hole
        AreaScalarParam{
            "polygon_with_hole",
            "POLYGON ((0 0, 0 2, 2 0, 0 0), (0.1 0.1, 0.1 0.5, 0.5 0.1, 0.1 0.1))",
            23744568445.094166}

        ),
    [](const ::testing::TestParamInfo<AreaScalarParam>& info) {
      return info.param.name;
    });
