#include "s2geography/tessellate.h"

#include <gtest/gtest.h>

#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/sedona_udf/sedona_udf_test_internal.h"

using namespace s2geography;

TEST(Tessellate, SedonaUdfTessellateToGeogArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::TessellateToGeog(&kernel);
  struct SedonaCScalarKernelImpl impl;
  // TessellateToGeog takes geometry (PLANAR) input, outputs geography
  // (SPHERICAL)
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB_PLANAR, NANOARROW_TYPE_DOUBLE},
      ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  // Use a very large tolerance (1e9 meters) so no tessellation occurs
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB_PLANAR, NANOARROW_TYPE_DOUBLE},
      {{"POINT (0 1)", "LINESTRING (0 1, 1 2)", std::nullopt}},
      {{1e9, 1e9, 1e9}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  // With large tolerance, output should match input (just converted to
  // geography)
  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 1)", "LINESTRING (0 1, 1 2)", std::nullopt}));
}

TEST(Tessellate, SedonaUdfTessellateToGeomArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::TessellateToGeom(&kernel);
  struct SedonaCScalarKernelImpl impl;
  // TessellateToGeom takes geography (SPHERICAL) input, outputs geometry
  // (PLANAR)
  ASSERT_NO_FATAL_FAILURE(
      TestInitKernel(&kernel, &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
                     ARROW_TYPE_WKB_PLANAR));

  nanoarrow::UniqueArray out_array;
  // Use a very large tolerance (1e9 meters) so no tessellation occurs
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
      {{"POINT (0 1)", "LINESTRING (0 1, 1 2)", std::nullopt}},
      {{1e9, 1e9, 1e9}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  // With large tolerance, output should match input (just converted to
  // geometry)
  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 1)", "LINESTRING (0 1, 1 2)", std::nullopt}));
}

TEST(Tessellate, SedonaUdfSegmentizeArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::Segmentize(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  // Use a very large segment length (1e9 meters) so no segmentization occurs
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
      {{"POINT (0 1)", "LINESTRING (0 1, 1 2)", std::nullopt}},
      {{1e9, 1e9, 1e9}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  // With large segment length, output should match input
  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 1)", "LINESTRING (0 1, 1 2)", std::nullopt}));
}

TEST(Tessellate, SedonaUdfSegmentizeWithSubdivision) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::Segmentize(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  // Use a small segment length to force subdivision
  // 111320 meters is approximately 1 degree at the equator
  // (Earth radius ~6371km, so 1 degree = 6371000 * pi/180 ≈ 111195m)
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
      {{"LINESTRING (0 0, 0 2)"}}, {{111320.0}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  // With ~1 degree max segment, a 2 degree line should be split into 2
  // segments
  ASSERT_NO_FATAL_FAILURE(
      TestResultGeography(out_array.get(), {"LINESTRING (0 0, 0 1, 0 2)"}));
}

// ============================================================================
// Parameterized Tests for TessellateToGeog
// ============================================================================

struct TessellateToGeogParam {
  std::string name;
  std::optional<std::string> input;
  std::optional<double> tolerance;
  std::optional<std::string> expected;

  friend std::ostream& operator<<(std::ostream& os,
                                  const TessellateToGeogParam& p) {
    os << (p.input ? *p.input : "null") << " with tolerance ";
    if (p.tolerance) {
      os << *p.tolerance;
    } else {
      os << "null";
    }
    os << " -> " << (p.expected ? *p.expected : "null");
    return os;
  }
};

class TessellateToGeogTest
    : public ::testing::TestWithParam<TessellateToGeogParam> {};

TEST_P(TessellateToGeogTest, SedonaUdf) {
  const auto& p = GetParam();

  struct SedonaCScalarKernel kernel;
  struct SedonaCScalarKernelImpl impl;
  s2geography::sedona_udf::TessellateToGeog(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB_PLANAR, NANOARROW_TYPE_DOUBLE},
      ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB_PLANAR, NANOARROW_TYPE_DOUBLE},
                        {{p.input}}, {{p.tolerance}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(out_array.get(), {p.expected}));
}

INSTANTIATE_TEST_SUITE_P(
    Tessellate, TessellateToGeogTest,
    ::testing::Values(
        // Nulls
        TessellateToGeogParam{"null_input", std::nullopt, 1e9, std::nullopt},
        TessellateToGeogParam{"null_tolerance", "POINT (0 0)", std::nullopt,
                              std::nullopt},
        TessellateToGeogParam{"null_both", std::nullopt, std::nullopt,
                              std::nullopt},

        // Empties
        TessellateToGeogParam{"empty_point", "POINT EMPTY", 1e9, "POINT EMPTY"},
        TessellateToGeogParam{"empty_linestring", "LINESTRING EMPTY", 1e9,
                              "LINESTRING EMPTY"},
        TessellateToGeogParam{"empty_polygon", "POLYGON EMPTY", 1e9,
                              "POLYGON EMPTY"},
        TessellateToGeogParam{"empty_multipoint", "MULTIPOINT EMPTY", 1e9,
                              "MULTIPOINT EMPTY"},
        TessellateToGeogParam{"empty_multilinestring", "MULTILINESTRING EMPTY",
                              1e9, "MULTILINESTRING EMPTY"},
        TessellateToGeogParam{"empty_multipolygon", "MULTIPOLYGON EMPTY", 1e9,
                              "MULTIPOLYGON EMPTY"},
        TessellateToGeogParam{"empty_geometrycollection",
                              "GEOMETRYCOLLECTION EMPTY", 1e9,
                              "GEOMETRYCOLLECTION EMPTY"},

        // Points (no tessellation needed regardless of tolerance)
        TessellateToGeogParam{"point_large_tol", "POINT (0 1)", 1e9,
                              "POINT (0 1)"},
        TessellateToGeogParam{"point_zm_large_tol", "POINT ZM (0 1 100 200)",
                              1e9, "POINT ZM (0 1 100 200)"},

        // Linestrings without tessellation (large tolerance)
        TessellateToGeogParam{"linestring_large_tol",
                              "LINESTRING (0 1, 1 2, 2 1)", 1e9,
                              "LINESTRING (0 1, 1 2, 2 1)"},
        TessellateToGeogParam{
            "linestring_zm_large_tol",
            "LINESTRING ZM (0 1 10 20, 1 2 30 40, 2 1 50 60)", 1e9,
            "LINESTRING ZM (0 1 10 20, 1 2 30 40, 2 1 50 60)"},

        // Polygons without tessellation (large tolerance)
        TessellateToGeogParam{"polygon_large_tol",
                              "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))", 1e9,
                              "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))"},
        TessellateToGeogParam{
            "polygon_zm_large_tol",
            "POLYGON ZM ((0 0 10 20, 1 0 30 40, 1 1 50 60, 0 1 70 80, "
            "0 0 10 20))",
            1e9,
            "POLYGON ZM ((0 0 10 20, 1 0 30 40, 1 1 50 60, 0 1 70 80, "
            "0 0 10 20))"},

        // MultiPoints (no tessellation needed)
        TessellateToGeogParam{"multipoint_large_tol",
                              "MULTIPOINT ((0 1), (1 2), (2 3))", 1e9,
                              "MULTIPOINT ((0 1), (1 2), (2 3))"},

        // MultiLinestrings without tessellation (large tolerance)
        TessellateToGeogParam{"multilinestring_large_tol",
                              "MULTILINESTRING ((0 1, 1 2), (2 3, 3 4))", 1e9,
                              "MULTILINESTRING ((0 1, 1 2), (2 3, 3 4))"},

        // MultiPolygons without tessellation (large tolerance)
        TessellateToGeogParam{"multipolygon_large_tol",
                              "MULTIPOLYGON (((0 0, 1 0, 1 1, 0 1, 0 0)), "
                              "((2 3, 3 3, 3 4, 2 4, 2 3)))",
                              1e9,
                              "MULTIPOLYGON (((0 0, 1 0, 1 1, 0 1, 0 0)), "
                              "((2 3, 3 3, 3 4, 2 4, 2 3)))"},

        // GeometryCollections without tessellation (large tolerance)
        TessellateToGeogParam{
            "geometrycollection_large_tol",
            "GEOMETRYCOLLECTION (POINT (0 1), LINESTRING (0 1, 1 2))", 1e9,
            "GEOMETRYCOLLECTION (POINT (0 1), LINESTRING (0 1, 1 2))"},

        // Tessellation with high-latitude horizontal line
        // At lat 45, a planar line stays at constant latitude but the geodesic
        // curves poleward, maximizing deviation and triggering tessellation
        TessellateToGeogParam{"linestring_tessellate_highlat",
                              "LINESTRING (-10 45, 10 45)", 10000.0,
                              "LINESTRING (-10 45, -5 45, 0 45, 5 45, 10 45)"},

        // Much smaller tolerance adds more intermediate points
        TessellateToGeogParam{
            "linestring_tessellate_highlat_small", "LINESTRING (-10 45, 10 45)",
            1000.0,
            "LINESTRING (-10 45, -7.5 45, -5 45, -2.5 45, 0 45, "
            "2.5 45, 5 45, 7.5 45, 10 45)"},

        // Multi-segment linestring - both segments at high latitude need
        // tessellation
        TessellateToGeogParam{"linestring_tessellate_multiseg",
                              "LINESTRING (-10 45, 10 45, 30 45)", 10000.0,
                              "LINESTRING (-10 45, -5 45, 0 45, 5 45, 10 45, "
                              "15 45, 20 45, 25 45, 30 45)"},

        // Z dimension - Z values should be linearly interpolated
        TessellateToGeogParam{
            "linestring_tessellate_highlat_z",
            "LINESTRING Z (-10 45 100, 10 45 200)", 10000.0,
            "LINESTRING Z (-10 45 100, -5 45 125.023904, 0 45 150, "
            "5 45 174.976096, 10 45 200)"},

        // M dimension - M values should be linearly interpolated
        TessellateToGeogParam{
            "linestring_tessellate_highlat_m",
            "LINESTRING M (-10 45 0, 10 45 100)", 10000.0,
            "LINESTRING M (-10 45 0, -5 45 25.023904, 0 45 50, "
            "5 45 74.976096, 10 45 100)"},

        // ZM dimension - both Z and M should be linearly interpolated
        TessellateToGeogParam{
            "linestring_tessellate_highlat_zm",
            "LINESTRING ZM (-10 45 100 0, 10 45 200 100)", 10000.0,
            "LINESTRING ZM (-10 45 100 0, -5 45 125.023904 25.023904, "
            "0 45 150 50, 5 45 174.976096 74.976096, 10 45 200 100)"}),
    [](const ::testing::TestParamInfo<TessellateToGeogParam>& info) {
      return info.param.name;
    });

// ============================================================================
// Parameterized Tests for TessellateToGeom
// ============================================================================

struct TessellateToGeomParam {
  std::string name;
  std::optional<std::string> input;
  std::optional<double> tolerance;
  std::optional<std::string> expected;

  friend std::ostream& operator<<(std::ostream& os,
                                  const TessellateToGeomParam& p) {
    os << (p.input ? *p.input : "null") << " with tolerance ";
    if (p.tolerance) {
      os << *p.tolerance;
    } else {
      os << "null";
    }
    os << " -> " << (p.expected ? *p.expected : "null");
    return os;
  }
};

class TessellateToGeomTest
    : public ::testing::TestWithParam<TessellateToGeomParam> {};

TEST_P(TessellateToGeomTest, SedonaUdf) {
  const auto& p = GetParam();

  struct SedonaCScalarKernel kernel;
  struct SedonaCScalarKernelImpl impl;
  s2geography::sedona_udf::TessellateToGeom(&kernel);

  ASSERT_NO_FATAL_FAILURE(
      TestInitKernel(&kernel, &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
                     ARROW_TYPE_WKB_PLANAR));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
                        {{p.input}}, {{p.tolerance}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(out_array.get(), {p.expected}));
}

INSTANTIATE_TEST_SUITE_P(
    Tessellate, TessellateToGeomTest,
    ::testing::Values(
        // Nulls
        TessellateToGeomParam{"null_input", std::nullopt, 1e9, std::nullopt},
        TessellateToGeomParam{"null_tolerance", "POINT (0 0)", std::nullopt,
                              std::nullopt},
        TessellateToGeomParam{"null_both", std::nullopt, std::nullopt,
                              std::nullopt},

        // Empties
        TessellateToGeomParam{"empty_point", "POINT EMPTY", 1e9, "POINT EMPTY"},
        TessellateToGeomParam{"empty_linestring", "LINESTRING EMPTY", 1e9,
                              "LINESTRING EMPTY"},
        TessellateToGeomParam{"empty_polygon", "POLYGON EMPTY", 1e9,
                              "POLYGON EMPTY"},
        TessellateToGeomParam{"empty_multipoint", "MULTIPOINT EMPTY", 1e9,
                              "MULTIPOINT EMPTY"},
        TessellateToGeomParam{"empty_multilinestring", "MULTILINESTRING EMPTY",
                              1e9, "MULTILINESTRING EMPTY"},
        TessellateToGeomParam{"empty_multipolygon", "MULTIPOLYGON EMPTY", 1e9,
                              "MULTIPOLYGON EMPTY"},
        TessellateToGeomParam{"empty_geometrycollection",
                              "GEOMETRYCOLLECTION EMPTY", 1e9,
                              "GEOMETRYCOLLECTION EMPTY"},

        // Points (no tessellation needed)
        TessellateToGeomParam{"point_large_tol", "POINT (0 1)", 1e9,
                              "POINT (0 1)"},
        TessellateToGeomParam{"point_zm_large_tol", "POINT ZM (0 1 100 200)",
                              1e9, "POINT ZM (0 1 100 200)"},

        // Linestrings without tessellation (large tolerance)
        TessellateToGeomParam{"linestring_large_tol",
                              "LINESTRING (0 1, 1 2, 2 1)", 1e9,
                              "LINESTRING (0 1, 1 2, 2 1)"},
        TessellateToGeomParam{
            "linestring_zm_large_tol",
            "LINESTRING ZM (0 1 10 20, 1 2 30 40, 2 1 50 60)", 1e9,
            "LINESTRING ZM (0 1 10 20, 1 2 30 40, 2 1 50 60)"},

        // Linestring across the antimeridian and back: returned longitudes
        // should return valid geometry with longitudes >180.
        TessellateToGeomParam{
            "linestring_large_tol_antimeridian",
            "LINESTRING (170 70, 175 70, -175 70, -175 -70, 175 -70, 170 -70)",
            1e9,
            "LINESTRING (170 70, 175 70, 185 70, 185 0, 185 -70, 175 -70, "
            "170 -70)"},
        TessellateToGeomParam{
            "linestring_large_tol_antimeridian_zm",
            "LINESTRING ZM (170 70 10 20, 175 70 30 40, -175 70 50 60, -175 "
            "-70 70 80, 175 -70 90 100, 170 -70 110 120)",
            1e9,
            "LINESTRING ZM (170 70 10 20, 175 70 30 40, 185 70 50 60, 185 0 "
            "60 70, 185 -70 70 80, 175 -70 90 100, 170 -70 110 120)"},

        // Polygons without tessellation (large tolerance)
        TessellateToGeomParam{"polygon_large_tol",
                              "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))", 1e9,
                              "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))"},
        TessellateToGeomParam{
            "polygon_zm_large_tol",
            "POLYGON ZM ((0 0 10 20, 1 0 30 40, 1 1 50 60, 0 1 70 80, "
            "0 0 10 20))",
            1e9,
            "POLYGON ZM ((0 0 10 20, 1 0 30 40, 1 1 50 60, 0 1 70 80, "
            "0 0 10 20))"},

        // MultiPoints (no tessellation needed)
        TessellateToGeomParam{"multipoint_large_tol",
                              "MULTIPOINT ((0 1), (1 2), (2 3))", 1e9,
                              "MULTIPOINT ((0 1), (1 2), (2 3))"},

        // MultiLinestrings without tessellation (large tolerance)
        TessellateToGeomParam{"multilinestring_large_tol",
                              "MULTILINESTRING ((0 1, 1 2), (2 3, 3 4))", 1e9,
                              "MULTILINESTRING ((0 1, 1 2), (2 3, 3 4))"},

        // MultiPolygons without tessellation (large tolerance)
        TessellateToGeomParam{"multipolygon_large_tol",
                              "MULTIPOLYGON (((0 0, 1 0, 1 1, 0 1, 0 0)), "
                              "((2 3, 3 3, 3 4, 2 4, 2 3)))",
                              1e9,
                              "MULTIPOLYGON (((0 0, 1 0, 1 1, 0 1, 0 0)), "
                              "((2 3, 3 3, 3 4, 2 4, 2 3)))"},

        // GeometryCollections without tessellation (large tolerance)
        TessellateToGeomParam{
            "geometrycollection_large_tol",
            "GEOMETRYCOLLECTION (POINT (0 1), LINESTRING (0 1, 1 2))", 1e9,
            "GEOMETRYCOLLECTION (POINT (0 1), LINESTRING (0 1, 1 2))"},

        // Tessellation with high-latitude horizontal line
        // At lat 45, the geodesic curves poleward from the constant-latitude
        // planar line, maximizing deviation and triggering tessellation
        TessellateToGeomParam{
            "linestring_tessellate_highlat", "LINESTRING (-10 45, 10 45)",
            10000.0,
            "LINESTRING (-10 45, -5.019332 45.328489, 0 45.438549, "
            "5.019332 45.328489, 10 45)"},

        // Much smaller tolerance adds more intermediate points
        TessellateToGeomParam{
            "linestring_tessellate_highlat_small", "LINESTRING (-10 45, 10 45)",
            1000.0,
            "LINESTRING (-10 45, -7.51685 45.191313, -5.019332 45.328489, "
            "-2.51211 45.411007, 0 45.438549, 2.51211 45.411007, "
            "5.019332 45.328489, 7.51685 45.191313, 10 45)"},

        // Multi-segment linestring - both segments at high latitude need
        // tessellation
        TessellateToGeomParam{
            "linestring_tessellate_multiseg",
            "LINESTRING (-10 45, 10 45, 30 45)", 10000.0,
            "LINESTRING (-10 45, -5.019332 45.328489, 0 45.438549, "
            "5.019332 45.328489, 10 45, 14.980668 45.328489, 20 45.438549, "
            "25.019332 45.328489, 30 45)"},

        // Tessellation across the antimeridian and back: returned longitudes
        // should return valid geometry with longitudes >180.
        TessellateToGeomParam{
            "linestring_tessellate_antimeridian",
            "LINESTRING (170 70, 175 70, -175 70, -175 -70, 175 -70, 170 -70)",
            1000.0,
            "LINESTRING (170 70, 172.5 70.017528, 175 70, 177.495787 "
            "70.052555, "
            "180 70.070104, 182.504213 70.052555, 185 70, 185 0, 185 -70, "
            "182.504213 -70.052555, 180 -70.070104, 177.495787 -70.052555, "
            "175 -70, 172.5 -70.017528, 170 -70)"},
        TessellateToGeomParam{
            "linestring_tessellate_antimeridian_zm",
            "LINESTRING ZM (170 70 10 20, 175 70 30 40, -175 70 50 60, -175 "
            "-70 70 80, 175 -70 90 100, 170 -70 110 120)",
            1000.0,
            "LINESTRING ZM (170 70 10 20, 172.5 70.017528 20 30, 175 70 30 40, "
            "177.495787 70.052555 34.991574 44.991574, 180 70.070104 40 50, "
            "182.504213 70.052555 45.008426 55.008426, 185 70 50 60, 185 0 60 "
            "70, 185 -70 70 80, 182.504213 -70.052555 74.991574 84.991574, "
            "180 -70.070104 80 90, 177.495787 -70.052555 85.008426 95.008426, "
            "175 -70 90 100, 172.5 -70.017528 100 110, 170 -70 110 120)"},

        // Z dimension - Z values should be linearly interpolated
        TessellateToGeomParam{
            "linestring_tessellate_highlat_z",
            "LINESTRING Z (-10 45 100, 10 45 200)", 10000.0,
            "LINESTRING Z (-10 45 100, -5.019332 45.328489 124.903342, "
            "0 45.438549 150, 5.019332 45.328489 175.096658, 10 45 200)"},

        // M dimension - M values should be linearly interpolated
        TessellateToGeomParam{
            "linestring_tessellate_highlat_m",
            "LINESTRING M (-10 45 0, 10 45 100)", 10000.0,
            "LINESTRING M (-10 45 0, -5.019332 45.328489 24.903342, "
            "0 45.438549 50, 5.019332 45.328489 75.096658, 10 45 100)"},

        // ZM dimension - both Z and M should be linearly interpolated
        TessellateToGeomParam{
            "linestring_tessellate_highlat_zm",
            "LINESTRING ZM (-10 45 100 0, 10 45 200 100)", 10000.0,
            "LINESTRING ZM (-10 45 100 0, -5.019332 45.328489 124.903342 "
            "24.903342, 0 45.438549 150 50, 5.019332 45.328489 175.096658 "
            "75.096658, 10 45 200 100)"}

        ),
    [](const ::testing::TestParamInfo<TessellateToGeomParam>& info) {
      return info.param.name;
    });

// ============================================================================
// Parameterized Tests for Segmentize
// ============================================================================

struct SegmentizeParam {
  std::string name;
  std::optional<std::string> input;
  std::optional<double> max_segment_length;
  std::optional<std::string> expected;

  friend std::ostream& operator<<(std::ostream& os, const SegmentizeParam& p) {
    os << (p.input ? *p.input : "null") << " with max_segment_length ";
    if (p.max_segment_length) {
      os << *p.max_segment_length;
    } else {
      os << "null";
    }
    os << " -> " << (p.expected ? *p.expected : "null");
    return os;
  }
};

class SegmentizeTest : public ::testing::TestWithParam<SegmentizeParam> {};

TEST_P(SegmentizeTest, SedonaUdf) {
  const auto& p = GetParam();

  struct SedonaCScalarKernel kernel;
  struct SedonaCScalarKernelImpl impl;
  s2geography::sedona_udf::Segmentize(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestInitKernel(
      &kernel, &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE}, {{p.input}},
      {{p.max_segment_length}}, out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(out_array.get(), {p.expected}));
}

// Earth radius in meters
constexpr double kEarthRadiusMeters = 6371000.0;
// 1 degree in meters at the equator
constexpr double kOneDegreeMeters = kEarthRadiusMeters * M_PI / 180.0;

INSTANTIATE_TEST_SUITE_P(
    Tessellate, SegmentizeTest,
    ::testing::Values(
        // Nulls
        SegmentizeParam{"null_input", std::nullopt, 1e9, std::nullopt},
        SegmentizeParam{"null_length", "POINT (0 0)", std::nullopt,
                        std::nullopt},
        SegmentizeParam{"null_both", std::nullopt, std::nullopt, std::nullopt},

        // Empties
        SegmentizeParam{"empty_point", "POINT EMPTY", 1e9, "POINT EMPTY"},
        SegmentizeParam{"empty_linestring", "LINESTRING EMPTY", 1e9,
                        "LINESTRING EMPTY"},
        SegmentizeParam{"empty_polygon", "POLYGON EMPTY", 1e9, "POLYGON EMPTY"},
        SegmentizeParam{"empty_multipoint", "MULTIPOINT EMPTY", 1e9,
                        "MULTIPOINT EMPTY"},
        SegmentizeParam{"empty_multilinestring", "MULTILINESTRING EMPTY", 1e9,
                        "MULTILINESTRING EMPTY"},
        SegmentizeParam{"empty_multipolygon", "MULTIPOLYGON EMPTY", 1e9,
                        "MULTIPOLYGON EMPTY"},
        SegmentizeParam{"empty_geometrycollection", "GEOMETRYCOLLECTION EMPTY",
                        1e9, "GEOMETRYCOLLECTION EMPTY"},

        // Points (no segmentation needed)
        SegmentizeParam{"point_large_seg", "POINT (0 1)", 1e9, "POINT (0 1)"},
        SegmentizeParam{"point_zm_large_seg", "POINT ZM (0 1 100 200)", 1e9,
                        "POINT ZM (0 1 100 200)"},

        // Linestrings without segmentation (large max segment)
        SegmentizeParam{"linestring_large_seg", "LINESTRING (0 1, 1 2, 2 1)",
                        1e9, "LINESTRING (0 1, 1 2, 2 1)"},
        SegmentizeParam{"linestring_zm_large_seg",
                        "LINESTRING ZM (0 1 10 20, 1 2 30 40, 2 1 50 60)", 1e9,
                        "LINESTRING ZM (0 1 10 20, 1 2 30 40, 2 1 50 60)"},

        // Polygons without segmentation (large max segment)
        SegmentizeParam{"polygon_large_seg",
                        "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))", 1e9,
                        "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))"},
        SegmentizeParam{
            "polygon_zm_large_seg",
            "POLYGON ZM ((0 0 10 20, 1 0 30 40, 1 1 50 60, 0 1 70 80, "
            "0 0 10 20))",
            1e9,
            "POLYGON ZM ((0 0 10 20, 1 0 30 40, 1 1 50 60, 0 1 70 80, "
            "0 0 10 20))"},

        // MultiPoints (no segmentation needed)
        SegmentizeParam{"multipoint_large_seg",
                        "MULTIPOINT ((0 1), (1 2), (2 3))", 1e9,
                        "MULTIPOINT ((0 1), (1 2), (2 3))"},

        // MultiLinestrings without segmentation (large max segment)
        SegmentizeParam{"multilinestring_large_seg",
                        "MULTILINESTRING ((0 1, 1 2), (2 3, 3 4))", 1e9,
                        "MULTILINESTRING ((0 1, 1 2), (2 3, 3 4))"},

        // MultiPolygons without segmentation (large max segment)
        SegmentizeParam{"multipolygon_large_seg",
                        "MULTIPOLYGON (((0 0, 1 0, 1 1, 0 1, 0 0)), "
                        "((2 3, 3 3, 3 4, 2 4, 2 3)))",
                        1e9,
                        "MULTIPOLYGON (((0 0, 1 0, 1 1, 0 1, 0 0)), "
                        "((2 3, 3 3, 3 4, 2 4, 2 3)))"},

        // GeometryCollections without segmentation (large max segment)
        SegmentizeParam{
            "geometrycollection_large_seg",
            "GEOMETRYCOLLECTION (POINT (0 1), LINESTRING (0 1, 1 2))", 1e9,
            "GEOMETRYCOLLECTION (POINT (0 1), LINESTRING (0 1, 1 2))"},

        // Segmentation - 2 degree line with ~1 degree max -> 2 segments
        SegmentizeParam{"linestring_2deg_split2", "LINESTRING (0 0, 0 2)",
                        kOneDegreeMeters * 1.1, "LINESTRING (0 0, 0 1, 0 2)"},

        // Segmentation - 3 degree line with ~1 degree max -> 3 segments
        SegmentizeParam{"linestring_3deg_split3", "LINESTRING (0 0, 0 3)",
                        kOneDegreeMeters * 1.1,
                        "LINESTRING (0 0, 0 1, 0 2, 0 3)"},

        // Segmentation - 4 degree line with ~1 degree max -> 4 segments
        SegmentizeParam{"linestring_4deg_split4", "LINESTRING (0 0, 0 4)",
                        kOneDegreeMeters * 1.1,
                        "LINESTRING (0 0, 0 1, 0 2, 0 3, 0 4)"},

        // Z dimension - Z values should be linearly interpolated
        SegmentizeParam{
            "linestring_2deg_split_z", "LINESTRING Z (0 0 100, 0 2 200)",
            kOneDegreeMeters * 1.1, "LINESTRING Z (0 0 100, 0 1 150, 0 2 200)"},

        // M dimension - M values should be linearly interpolated
        SegmentizeParam{"linestring_2deg_split_m",
                        "LINESTRING M (0 0 0, 0 2 100)", kOneDegreeMeters * 1.1,
                        "LINESTRING M (0 0 0, 0 1 50, 0 2 100)"},

        // ZM dimension - both Z and M should be linearly interpolated
        SegmentizeParam{"linestring_2deg_split_zm",
                        "LINESTRING ZM (0 0 100 0, 0 2 200 100)",
                        kOneDegreeMeters * 1.1,
                        "LINESTRING ZM (0 0 100 0, 0 1 150 50, 0 2 200 100)"},

        // Polygon segmentation
        // Note: The midpoint of the edge from (0,2) to (2,2) is at latitude
        // 2.000304 due to the great circle path curving slightly poleward
        SegmentizeParam{
            "polygon_2deg_split", "POLYGON ((0 0, 0 2, 2 2, 2 0, 0 0))",
            kOneDegreeMeters * 1.1,
            "POLYGON ((0 0, 0 1, 0 2, 1 2.000304, 2 2, 2 1, 2 0, 1 0, 0 0))"}

        ),
    [](const ::testing::TestParamInfo<SegmentizeParam>& info) {
      return info.param.name;
    });
