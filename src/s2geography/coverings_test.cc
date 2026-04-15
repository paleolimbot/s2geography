#include "s2geography/coverings.h"

#include <gtest/gtest.h>
#include <s2/s2cell_id.h>
#include <s2/s2latlng.h>

#include <optional>
#include <string>

#include "s2geography/geoarrow-geography.h"
#include "s2geography/sedona_udf/sedona_udf_test_internal.h"

using namespace s2geography;

// Test parameter structure for LatLngRectBounder
struct LatLngRectBounderParam {
  std::string name;
  std::string wkt;
  std::optional<double> xmin;
  std::optional<double> ymin;
  std::optional<double> xmax;
  std::optional<double> ymax;

  friend std::ostream& operator<<(std::ostream& os,
                                  const LatLngRectBounderParam& p) {
    os << p.name;
    return os;
  }
};

class LatLngRectBounderTest
    : public ::testing::TestWithParam<LatLngRectBounderParam> {};

TEST_P(LatLngRectBounderTest, Bounds) {
  const auto& p = GetParam();

  auto test_geom = TestGeometry::FromWKT(p.wkt);
  GeoArrowGeography geog;
  geog.Init(test_geom.geom());

  LatLngRectBounder bounder;
  bounder.Clear();
  bounder.Update(geog);
  S2LatLngRect bounds = bounder.Finish();

  if (!p.xmin.has_value()) {
    // Expect empty bounds
    EXPECT_TRUE(bounds.is_empty()) << "Expected empty bounds for " << p.wkt;
  } else {
    EXPECT_FALSE(bounds.is_empty())
        << "Expected non-empty bounds for " << p.wkt;

    EXPECT_DOUBLE_EQ(bounds.lng_lo().degrees(), *p.xmin)
        << "xmin mismatch for " << p.wkt;
    EXPECT_DOUBLE_EQ(bounds.lat_lo().degrees(), *p.ymin)
        << "ymin mismatch for " << p.wkt;
    EXPECT_DOUBLE_EQ(bounds.lng_hi().degrees(), *p.xmax)
        << "xmax mismatch for " << p.wkt;
    EXPECT_DOUBLE_EQ(bounds.lat_hi().degrees(), *p.ymax)
        << "ymax mismatch for " << p.wkt;
  }
}

INSTANTIATE_TEST_SUITE_P(
    Coverings, LatLngRectBounderTest,
    ::testing::Values(
        // Empty geometries
        LatLngRectBounderParam{"point_empty", "POINT EMPTY", std::nullopt,
                               std::nullopt, std::nullopt, std::nullopt},
        LatLngRectBounderParam{"linestring_empty", "LINESTRING EMPTY",
                               std::nullopt, std::nullopt, std::nullopt,
                               std::nullopt},
        LatLngRectBounderParam{"polygon_empty", "POLYGON EMPTY", std::nullopt,
                               std::nullopt, std::nullopt, std::nullopt},

        // Points (note: longitude is expanded by 2*DBL_EPSILON radians for
        // rounding error)
        LatLngRectBounderParam{"point_origin", "POINT (0 0)",
                               -2.5444437451708134e-14, 0.0,
                               2.5444437451708134e-14, 0.0},
        LatLngRectBounderParam{
            "multipoint_spanning", "MULTIPOINT ((-10 -20), (30 40))",
            -10.000000000000025, -20.0, 30.000000000000025, 40.0},
        LatLngRectBounderParam{"multipoint_antimeridian",
                               "MULTIPOINT ((170 10), (-170 11))", 170.0, 10.0,
                               -170.0, 11.0},

        // LineStrings
        LatLngRectBounderParam{
            "linestring_horizontal_equator", "LINESTRING (0 0, 10 0)", 0.0,
            -4.1493099444912135e-14, 10.0, 4.1493099444912135e-14},
        LatLngRectBounderParam{"linestring_vertical", "LINESTRING (0 0, 0 10)",
                               0.0, -2.5444437451708134e-14, 0.0,
                               10.000000000000025},
        LatLngRectBounderParam{"linestring_diagonal", "LINESTRING (0 1, 10 11)",
                               0.0, 0.99999999999997458, 10.0,
                               11.000000000000025},
        LatLngRectBounderParam{"linestring_north_pole",
                               "LINESTRING (90 80, -90 80)", -180.0, 80.0,
                               180.0, 90.0},
        LatLngRectBounderParam{"linestring_south_pole",
                               "LINESTRING (90 -80, -90 -80)", -180.0, -90.0,
                               180.0, -80.0},
        LatLngRectBounderParam{"linestring_antimeridian",
                               "LINESTRING (170 10, -170 10)", 170.0,
                               9.9999999999999787, -170.0, 10.151081711048198},
        LatLngRectBounderParam{
            "multilinestring_antimeridian",
            "MULTILINESTRING ((160 10, 170 10), (-170 10, -160 10))", 160.0,
            9.9999999999999787, -160.0, 10.037423045910776},

        // Polygons
        LatLngRectBounderParam{"triangle", "POLYGON ((0 0, 10 0, 5 10, 0 0))",
                               0.0, -4.1493099444912135e-14, 10.0,
                               10.000000000000025},
        LatLngRectBounderParam{"triangle_north_pole",
                               "POLYGON ((0 80, 120 80, -120 80, 0 80))",
                               -180.0, 80.0, 180.0, 90.0},
        LatLngRectBounderParam{"triangle_south_pole",
                               "POLYGON ((0 -80, -120 -80, 120 -80, 0 -80))",
                               -180.0, -90.0, 180.0, -80.0}

        ));

// Test that Clear() resets state properly
TEST(LatLngRectBounderTest, ClearResetsState) {
  auto test_geom = TestGeometry::FromWKT("POINT (10 20)");
  GeoArrowGeography geog;
  geog.Init(test_geom.geom());

  LatLngRectBounder bounder;
  bounder.Clear();
  bounder.Update(geog);

  S2LatLngRect bounds1 = bounder.Finish();
  ASSERT_FALSE(bounds1.is_empty());

  // Clear and verify it's empty again
  bounder.Clear();
  S2LatLngRect bounds2 = bounder.Finish();
  EXPECT_TRUE(bounds2.is_empty());
}

// Test that multiple Update() calls accumulate bounds
TEST(LatLngRectBounderTest, AccumulatesBounds) {
  auto geom1 = TestGeometry::FromWKT("POINT (0 0)");
  auto geom2 = TestGeometry::FromWKT("POINT (10 20)");

  GeoArrowGeography geog1, geog2;
  geog1.Init(geom1.geom());
  geog2.Init(geom2.geom());

  LatLngRectBounder bounder;
  bounder.Clear();
  bounder.Update(geog1);
  bounder.Update(geog2);
  S2LatLngRect bounds = bounder.Finish();

  EXPECT_DOUBLE_EQ(bounds.lng_lo().degrees(), -2.5444437451708134e-14);
  EXPECT_DOUBLE_EQ(bounds.lat_lo().degrees(), 0.0);
  EXPECT_DOUBLE_EQ(bounds.lng_hi().degrees(), 10.000000000000025);
  EXPECT_DOUBLE_EQ(bounds.lat_hi().degrees(), 20.0);
}

TEST(Coverings, SedonaUdfCellIdFromPointArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::CellIdFromPointKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(
      TestInitKernel(&kernel, &impl, {ARROW_TYPE_WKB}, NANOARROW_TYPE_INT64));

  // Compute expected cell IDs
  S2CellId id_origin(S2LatLng::FromDegrees(0, 0).ToPoint());
  S2CellId id_point1(S2LatLng::FromDegrees(1, 0).ToPoint());

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB},
      {{"POINT (0 0)", "POINT (0 1)", "POINT EMPTY", std::nullopt}}, {},
      out_array.get()));
  impl.release(&impl);
  kernel.release(&kernel);

  // We use doubles to test as the expected value to simplify the signature of
  // TestResultArrow (even though it is usually dubious to do this type of
  // cast unchecked)
  ASSERT_NO_FATAL_FAILURE(TestResultArrow(
      out_array.get(), NANOARROW_TYPE_INT64,
      {static_cast<double>(id_origin.id()), static_cast<double>(id_point1.id()),
       std::nullopt, std::nullopt}));
}

TEST(Coverings, SedonaUdfCoveringCellIdsArray) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::CoveringCellIdsKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(
      TestInitKernel(&kernel, &impl, {ARROW_TYPE_WKB}, NANOARROW_TYPE_LIST));

  // Linestring spanning ~100 degrees should require multiple cells
  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteKernel(
      &impl, {ARROW_TYPE_WKB},
      {{"POINT (0 0)", "POINT (0 1)", "LINESTRING (0 0, 100 50)", "POINT EMPTY",
        std::nullopt}},
      {}, out_array.get()));

  // Check the list structure
  ASSERT_EQ(out_array->length, 5);
  ASSERT_EQ(out_array->n_children, 1);
  ASSERT_NE(out_array->buffers[1], nullptr);

  // Get offsets (int32 for NANOARROW_TYPE_LIST)
  auto* offsets = reinterpret_cast<const int32_t*>(out_array->buffers[1]);
  // Row 0: POINT (0 0) - should have 1 cell
  EXPECT_EQ(offsets[1] - offsets[0], 1);
  // Row 1: POINT (0 1) - should have 1 cell
  EXPECT_EQ(offsets[2] - offsets[1], 1);
  // Row 2: LINESTRING (0 0, 100 50) - should have max_cells
  int linestring_cells = offsets[3] - offsets[2];
  EXPECT_EQ(linestring_cells, 8);
  // Row 3: POINT EMPTY - should have 0 cells
  EXPECT_EQ(offsets[4] - offsets[3], 0);
  // Row 4: null - should have 0 cells
  EXPECT_EQ(offsets[5] - offsets[4], 0);

  // Check child array contains valid cell IDs
  auto* child = out_array->children[0];
  ASSERT_NE(child, nullptr);
  int total_cells = 1 + 1 + linestring_cells;
  EXPECT_EQ(child->length, total_cells);
  ASSERT_NE(child->buffers[1], nullptr);
  auto* cell_ids = reinterpret_cast<const int64_t*>(child->buffers[1]);

  // The point cell IDs should correspond to the input points
  S2CellId id0(cell_ids[0]);
  S2CellId id1(cell_ids[1]);

  S2CellId expected_id0(S2LatLng::FromDegrees(0, 0).ToPoint());
  S2CellId expected_id1(S2LatLng::FromDegrees(1, 0).ToPoint());
  EXPECT_EQ(id0, expected_id0);
  EXPECT_EQ(id1, expected_id1);

  impl.release(&impl);
  kernel.release(&kernel);
}

TEST(Coverings, SedonaUdfBoundingBox) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::BoundingBoxKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(
      TestInitKernel(&kernel, &impl, {ARROW_TYPE_WKB}, NANOARROW_TYPE_STRUCT));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB},
                        {{"POINT (0 0)", "MULTIPOINT ((-10 -20), (30 40))",
                          "POINT EMPTY", std::nullopt}},
                        {}, out_array.get()));

  // Check the struct has 4 rows and 4 children (xmin, ymin, xmax, ymax)
  ASSERT_EQ(out_array->length, 4);
  ASSERT_EQ(out_array->n_children, 4);

  // For struct output, nulls are at the struct level, not in children.
  ASSERT_NE(out_array->buffers[0], nullptr);  // validity bitmap should exist
  auto* validity = static_cast<const uint8_t*>(out_array->buffers[0]);
  EXPECT_TRUE(ArrowBitGet(validity, 0));
  EXPECT_TRUE(ArrowBitGet(validity, 1));
  EXPECT_FALSE(ArrowBitGet(validity, 2));
  EXPECT_FALSE(ArrowBitGet(validity, 3));

  // Test children values for non-null rows using TestResultArrow
  // Note: Children have placeholder values (0.0) for null struct rows,
  // so we only check the non-null rows' values
  // Child 0: xmin
  ASSERT_NO_FATAL_FAILURE(TestResultArrow(
      out_array->children[0], NANOARROW_TYPE_DOUBLE,
      {-2.5444437451708134e-14, -10.000000000000025, 0.0, 0.0}));
  // Child 1: ymin
  ASSERT_NO_FATAL_FAILURE(TestResultArrow(
      out_array->children[1], NANOARROW_TYPE_DOUBLE, {0.0, -20.0, 0.0, 0.0}));
  // Child 2: xmax
  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array->children[2], NANOARROW_TYPE_DOUBLE,
                      {2.5444437451708134e-14, 30.000000000000025, 0.0, 0.0}));
  // Child 3: ymax
  ASSERT_NO_FATAL_FAILURE(TestResultArrow(
      out_array->children[3], NANOARROW_TYPE_DOUBLE, {0.0, 40.0, 0.0, 0.0}));

  impl.release(&impl);
  kernel.release(&kernel);
}
