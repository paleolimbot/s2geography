#include "s2geography/coverings.h"

#include <gtest/gtest.h>

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
    EXPECT_FALSE(bounds.is_empty()) << "Expected non-empty bounds for " << p.wkt;

    // Use larger tolerance for polygons due to geodesic edge bulging
    constexpr double kTolerance = 0.1;  // degrees
    EXPECT_NEAR(bounds.lng_lo().degrees(), *p.xmin, kTolerance)
        << "xmin mismatch for " << p.wkt;
    EXPECT_NEAR(bounds.lat_lo().degrees(), *p.ymin, kTolerance)
        << "ymin mismatch for " << p.wkt;
    EXPECT_NEAR(bounds.lng_hi().degrees(), *p.xmax, kTolerance)
        << "xmax mismatch for " << p.wkt;
    EXPECT_NEAR(bounds.lat_hi().degrees(), *p.ymax, kTolerance)
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

        // Points
        LatLngRectBounderParam{"point_origin", "POINT (0 0)", 0.0, 0.0, 0.0,
                               0.0},
        LatLngRectBounderParam{"point_positive", "POINT (10 20)", 10.0, 20.0,
                               10.0, 20.0},
        LatLngRectBounderParam{"point_negative", "POINT (-30 -40)", -30.0,
                               -40.0, -30.0, -40.0},

        // MultiPoints
        LatLngRectBounderParam{"multipoint_two", "MULTIPOINT ((0 0), (10 20))",
                               0.0, 0.0, 10.0, 20.0},
        LatLngRectBounderParam{
            "multipoint_spanning", "MULTIPOINT ((-10 -20), (30 40))", -10.0,
            -20.0, 30.0, 40.0},

        // LineStrings
        LatLngRectBounderParam{"linestring_horizontal",
                               "LINESTRING (0 0, 10 0)", 0.0, 0.0, 10.0, 0.0},
        LatLngRectBounderParam{"linestring_vertical", "LINESTRING (0 0, 0 10)",
                               0.0, 0.0, 0.0, 10.0},
        LatLngRectBounderParam{"linestring_diagonal",
                               "LINESTRING (0 0, 10 10)", 0.0, 0.0, 10.0, 10.0},

        // Polygons (simple cases)
        LatLngRectBounderParam{"triangle", "POLYGON ((0 0, 10 0, 5 10, 0 0))",
                               0.0, 0.0, 10.0, 10.0},
        LatLngRectBounderParam{"square", "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
                               0.0, 0.0, 10.0, 10.0}));

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

  constexpr double kTolerance = 1e-6;
  EXPECT_NEAR(bounds.lng_lo().degrees(), 0.0, kTolerance);
  EXPECT_NEAR(bounds.lat_lo().degrees(), 0.0, kTolerance);
  EXPECT_NEAR(bounds.lng_hi().degrees(), 10.0, kTolerance);
  EXPECT_NEAR(bounds.lat_hi().degrees(), 20.0, kTolerance);
}
