#include "s2geography/wkb-geography.h"

#include <gtest/gtest.h>

#include <string>

#include "geoarrow/geoarrow.hpp"
#include "s2geography/geography.h"

using namespace s2geography;

class TestGeometry {
 public:
  TestGeometry() {
    GEOARROW_THROW_NOT_OK(nullptr, GeoArrowGeometryInit(&geom_));
  }

  explicit TestGeometry(std::string_view wkt) : TestGeometry() {
    struct GeoArrowStringView wkt_view{wkt.data(),
                                       static_cast<int64_t>(wkt.size())};

    struct GeoArrowVisitor v;
    GeoArrowVisitorInitVoid(&v);
    GeoArrowGeometryInitVisitor(&geom_, &v);

    struct GeoArrowWKTReader reader;
    GEOARROW_THROW_NOT_OK(nullptr, GeoArrowWKTReaderInit(&reader));
    GeoArrowErrorCode code = GeoArrowWKTReaderVisit(&reader, wkt_view, &v);
    GeoArrowWKTReaderReset(&reader);
    if (code != GEOARROW_OK) {
      throw Exception("Invalid WKT");
    }
  }

  struct GeoArrowGeometryView geom() { return GeoArrowGeometryAsView(&geom_); }

  ~TestGeometry() { GeoArrowGeometryReset(&geom_); }

 private:
  struct GeoArrowGeometry geom_;
};

TEST(GeoArrowLaxPolylineShape, DefaultConstructor) {
  GeoArrowLaxPolylineShape shape;
  EXPECT_EQ(shape.num_edges(), 0);
  EXPECT_EQ(shape.dimension(), 1);
  EXPECT_EQ(shape.num_chains(), 0);
}

TEST(GeoArrowLaxPolylineShape, Linestring) {
  TestGeometry geom("LINESTRING (0 0, 0 1, 1 0)");
  GeoArrowLaxPolylineShape shape(geom.geom());
  EXPECT_EQ(shape.num_edges(), 2);
  EXPECT_EQ(shape.dimension(), 1);
  EXPECT_EQ(shape.num_chains(), 1);
}
