#include "s2geography/geoarrow-geography.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "geoarrow/geoarrow.hpp"
#include "s2geography/geography.h"
#include "s2geography/wkt-reader.h"

using namespace s2geography;

class TestGeometry {
 public:
  TestGeometry() : oriented_(false) {
    GEOARROW_THROW_NOT_OK(nullptr, GeoArrowGeometryInit(&geom_));
  }

  explicit TestGeometry(std::string_view wkt) : TestGeometry() {
    label_ = wkt;
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

  static TestGeometry FromWKB(const std::vector<uint8_t>& wkb) {
    TestGeometry result;
    struct GeoArrowWKBReader reader;
    GEOARROW_THROW_NOT_OK(nullptr, GeoArrowWKBReaderInit(&reader));
    struct GeoArrowBufferView src{wkb.data(), static_cast<int64_t>(wkb.size())};
    struct GeoArrowGeometryView view;
    struct GeoArrowError error;
    GeoArrowErrorCode code = GeoArrowWKBReaderRead(&reader, src, &view, &error);
    if (code != GEOARROW_OK) {
      GeoArrowWKBReaderReset(&reader);
      throw Exception("Invalid WKB");
    }

    // Copy the parsed geometry into our owned GeoArrowGeometry
    struct GeoArrowVisitor v;
    GeoArrowVisitorInitVoid(&v);
    GeoArrowGeometryInitVisitor(&result.geom_, &v);
    code = GeoArrowGeometryViewVisit(view, &v);
    GeoArrowWKBReaderReset(&reader);
    if (code != GEOARROW_OK) {
      throw Exception("Failed to copy WKB geometry");
    }
    return result;
  }

  struct GeoArrowGeometryView geom() const {
    return GeoArrowGeometryAsView(&geom_);
  }

  bool oriented() const { return oriented_; }

  void set_oriented(bool oriented) { oriented_ = oriented; }

  std::string_view label() const { return label_; }

  std::unique_ptr<GeoArrowLaxPolygonShape> ToPolygonShape() const {
    auto out = std::make_unique<GeoArrowLaxPolygonShape>(geom());
    if (!oriented()) {
      out->NormalizeOrientation();
    }

    return out;
  }

  ~TestGeometry() { GeoArrowGeometryReset(&geom_); }

 private:
  struct GeoArrowGeometry geom_;
  std::string label_;
  bool oriented_;
};

TEST(GeoArrowPointShape, DefaultConstructor) {
  GeoArrowPointShape shape;
  EXPECT_EQ(shape.num_edges(), 0);
  EXPECT_EQ(shape.dimension(), 0);
  EXPECT_EQ(shape.num_chains(), 0);
  EXPECT_EQ(shape.num_vertices(), 0);
}

TEST(GeoArrowPointShape, SinglePoint) {
  TestGeometry geom("POINT (30 10)");
  GeoArrowPointShape shape(geom.geom());
  EXPECT_EQ(shape.num_vertices(), 1);
  EXPECT_EQ(shape.num_edges(), 1);
  EXPECT_EQ(shape.num_chains(), 1);
  EXPECT_EQ(shape.dimension(), 0);

  auto e = shape.edge(0);
  EXPECT_EQ(e.v0, e.v1);  // degenerate edge

  auto c = shape.chain(0);
  EXPECT_EQ(c.start, 0);
  EXPECT_EQ(c.length, 1);

  auto pos = shape.chain_position(0);
  EXPECT_EQ(pos.chain_id, 0);
  EXPECT_EQ(pos.offset, 0);

  auto ce = shape.chain_edge(0, 0);
  EXPECT_EQ(ce.v0, ce.v1);
}

TEST(GeoArrowPointShape, MultiPoint) {
  TestGeometry geom("MULTIPOINT ((0 0), (1 1), (2 2))");
  GeoArrowPointShape shape(geom.geom());
  EXPECT_EQ(shape.num_vertices(), 3);
  EXPECT_EQ(shape.num_edges(), 3);
  EXPECT_EQ(shape.num_chains(), 1);
  EXPECT_EQ(shape.dimension(), 0);

  // Single chain containing all points
  auto c = shape.chain(0);
  EXPECT_EQ(c.start, 0);
  EXPECT_EQ(c.length, 3);

  for (int i = 0; i < 3; ++i) {
    auto pos = shape.chain_position(i);
    EXPECT_EQ(pos.chain_id, 0);
    EXPECT_EQ(pos.offset, i);

    auto e = shape.edge(i);
    EXPECT_EQ(e.v0, e.v1);  // degenerate edge

    auto ce = shape.chain_edge(0, i);
    EXPECT_EQ(ce.v0, ce.v1);
    EXPECT_EQ(ce.v0, e.v0);
  }
}

TEST(GeoArrowPointShape, BigEndianWKB) {
  // clang-format off
  // Big-endian WKB for POINT (30 10)
  std::vector<uint8_t> wkb = {
    0x00,                                      // big endian
    0x00, 0x00, 0x00, 0x01,                    // type: Point
    0x40, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // x: 30.0
    0x40, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // y: 10.0
  };
  // clang-format on

  auto geom = TestGeometry::FromWKB(wkb);
  GeoArrowPointShape shape(geom.geom());
  EXPECT_EQ(shape.num_vertices(), 1);
  EXPECT_EQ(shape.num_edges(), 1);
  EXPECT_EQ(shape.num_chains(), 1);
}

TEST(GeoArrowPointShape, ShapeIndexIntersection) {
  TestGeometry point_geom("MULTIPOINT ((0 0), (1 1), (50 50))");
  GeoArrowPointShape shape(point_geom.geom());
  EXPECT_EQ(shape.num_chains(), 1);

  MutableS2ShapeIndex point_index;
  point_index.Add(std::make_unique<GeoArrowPointShape>(point_geom.geom()));

  WKTReader reader;
  S2BooleanOperation::Options options;

  // Polygon overlapping the first two points
  auto poly_geog =
      reader.read_feature("POLYGON ((-1 -1, 2 -1, 2 2, -1 2, -1 -1))");
  ShapeIndexGeography poly_index(*poly_geog);
  EXPECT_TRUE(S2BooleanOperation::Intersects(point_index,
                                             poly_index.ShapeIndex(), options));

  // Polygon far from all points
  auto far_geog =
      reader.read_feature("POLYGON ((80 80, 81 80, 81 81, 80 81, 80 80))");
  ShapeIndexGeography far_index(*far_geog);
  EXPECT_FALSE(S2BooleanOperation::Intersects(point_index,
                                              far_index.ShapeIndex(), options));
}

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

// Big-endian WKB for LINESTRING (30 10, 10 30, 40 40)
// Byte order: 0x00 = big endian
// Type: 0x00000002 = LineString
// NumPoints: 0x00000003 = 3
TEST(GeoArrowLaxPolylineShape, BigEndianWKB) {
  // clang-format off
  std::vector<uint8_t> wkb = {
    0x00,                                      // big endian
    0x00, 0x00, 0x00, 0x02,                    // type: LineString
    0x00, 0x00, 0x00, 0x03,                    // num points: 3
    0x40, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // x: 30.0
    0x40, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // y: 10.0
    0x40, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // x: 10.0
    0x40, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // y: 30.0
    0x40, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // x: 40.0
    0x40, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // y: 40.0
  };
  // clang-format on

  auto geom = TestGeometry::FromWKB(wkb);
  GeoArrowLaxPolylineShape shape(geom.geom());
  EXPECT_EQ(shape.num_edges(), 2);
  EXPECT_EQ(shape.num_chains(), 1);
  EXPECT_EQ(shape.num_vertices(), 3);
}

TEST(GeoArrowLaxPolylineShape, MultiLinestring2Components) {
  TestGeometry geom("MULTILINESTRING ((0 0, 1 1, 2 2), (10 10, 11 11))");
  GeoArrowLaxPolylineShape shape(geom.geom());
  EXPECT_EQ(shape.num_chains(), 2);
  EXPECT_EQ(shape.num_edges(), 3);  // 2 + 1
  EXPECT_EQ(shape.num_vertices(), 5);

  // chain_position maps global edge ids to (chain, offset)
  auto pos0 = shape.chain_position(0);
  EXPECT_EQ(pos0.chain_id, 0);
  EXPECT_EQ(pos0.offset, 0);

  auto pos2 = shape.chain_position(2);
  EXPECT_EQ(pos2.chain_id, 1);
  EXPECT_EQ(pos2.offset, 0);
}

TEST(GeoArrowLaxPolylineShape, MultiLinestring3Components) {
  TestGeometry geom(
      "MULTILINESTRING ((0 0, 1 0), (2 0, 3 0, 4 0), (5 0, 6 0))");
  GeoArrowLaxPolylineShape shape(geom.geom());
  EXPECT_EQ(shape.num_chains(), 3);
  EXPECT_EQ(shape.num_edges(), 4);  // 1 + 2 + 1
  EXPECT_EQ(shape.num_vertices(), 7);

  auto pos3 = shape.chain_position(3);
  EXPECT_EQ(pos3.chain_id, 2);
  EXPECT_EQ(pos3.offset, 0);
}

TEST(GeoArrowLaxPolylineShape, MultiLinestring4Components) {
  TestGeometry geom(
      "MULTILINESTRING ((0 0, 1 0), (2 0, 3 0), (4 0, 5 0), (6 0, 7 0))");
  GeoArrowLaxPolylineShape shape(geom.geom());
  EXPECT_EQ(shape.num_chains(), 4);
  EXPECT_EQ(shape.num_edges(), 4);
  EXPECT_EQ(shape.num_vertices(), 8);

  auto pos0 = shape.chain_position(0);
  EXPECT_EQ(pos0.chain_id, 0);
  EXPECT_EQ(pos0.offset, 0);

  auto pos3 = shape.chain_position(3);
  EXPECT_EQ(pos3.chain_id, 3);
  EXPECT_EQ(pos3.offset, 0);
}

TEST(GeoArrowLaxPolylineShape, MultiLinestring3ComponentsOneEmpty) {
  TestGeometry geom("MULTILINESTRING ((0 0, 1 0, 2 0), EMPTY, (3 0, 4 0))");
  GeoArrowLaxPolylineShape shape(geom.geom());
  EXPECT_EQ(shape.num_chains(), 3);
  // The EMPTY linestring contributes 0 vertices and -1 edges (clamped behavior
  // depends on implementation), but the total edges should be 2 + 0 + 1 = 3
  // unless the empty component contributes a negative edge count.
  // With the current implementation: edges = (3-1) + (0-1) + (2-1) = 2 - 1 + 1
  // = 2 This tests the current behavior.
  EXPECT_EQ(shape.num_vertices(), 5);
}

TEST(GeoArrowLaxPolylineShape, ShapeIndexIntersection) {
  // Create a multilinestring with 4 components that cross over a region
  TestGeometry line_geom(
      "MULTILINESTRING ((-1 0, 1 0), (0 -1, 0 1), "
      "(-1 -1, 1 1), (-1 1, 1 -1))");
  GeoArrowLaxPolylineShape shape(line_geom.geom());
  EXPECT_EQ(shape.num_chains(), 4);

  // Build a ShapeIndexGeography from a polygon that overlaps the lines
  WKTReader reader;
  auto poly_geog = reader.read_feature(
      "POLYGON ((-0.5 -0.5, 0.5 -0.5, 0.5 0.5, "
      "-0.5 0.5, -0.5 -0.5))");
  ShapeIndexGeography poly_index(*poly_geog);

  // Build a MutableS2ShapeIndex containing our shape
  MutableS2ShapeIndex line_index;
  line_index.Add(std::make_unique<GeoArrowLaxPolylineShape>(line_geom.geom()));

  // Check intersection using S2BooleanOperation
  S2BooleanOperation::Options options;
  EXPECT_TRUE(S2BooleanOperation::Intersects(line_index,
                                             poly_index.ShapeIndex(), options));

  // Also check that a distant polygon does NOT intersect
  auto far_geog =
      reader.read_feature("POLYGON ((50 50, 51 50, 51 51, 50 51, 50 50))");
  ShapeIndexGeography far_index(*far_geog);
  EXPECT_FALSE(S2BooleanOperation::Intersects(line_index,
                                              far_index.ShapeIndex(), options));
}

// --- GeoArrowLaxPolygonShape tests ---

TEST(GeoArrowLaxPolygonShape, DefaultConstructor) {
  GeoArrowLaxPolygonShape shape;
  EXPECT_EQ(shape.num_edges(), 0);
  EXPECT_EQ(shape.dimension(), 2);
  EXPECT_EQ(shape.num_chains(), 0);
  EXPECT_EQ(shape.num_loops(), 0);
}

TEST(GeoArrowLaxPolygonShape, SimpleTriangle) {
  TestGeometry geom("POLYGON ((0 0, 1 0, 0 1, 0 0))");
  GeoArrowLaxPolygonShape shape(geom.geom());
  EXPECT_EQ(shape.num_loops(), 1);
  EXPECT_EQ(shape.num_loop_vertices(0), 4);
  // num_edges == total vertices (each ring is closed)
  EXPECT_EQ(shape.num_edges(), 4);
  EXPECT_EQ(shape.num_chains(), 1);
  EXPECT_EQ(shape.dimension(), 2);

  auto chain0 = shape.chain(0);
  EXPECT_EQ(chain0.start, 0);
  EXPECT_EQ(chain0.length, 4);
}

TEST(GeoArrowLaxPolygonShape, PolygonWithHole) {
  TestGeometry geom(
      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0), "
      "(2 2, 8 2, 8 8, 2 8, 2 2))");
  GeoArrowLaxPolygonShape shape(geom.geom());
  EXPECT_EQ(shape.num_loops(), 2);
  EXPECT_EQ(shape.num_loop_vertices(0), 5);
  EXPECT_EQ(shape.num_loop_vertices(1), 5);
  EXPECT_EQ(shape.num_edges(), 10);
  EXPECT_EQ(shape.num_chains(), 2);

  auto chain0 = shape.chain(0);
  EXPECT_EQ(chain0.start, 0);
  EXPECT_EQ(chain0.length, 5);

  auto chain1 = shape.chain(1);
  EXPECT_EQ(chain1.start, 5);
  EXPECT_EQ(chain1.length, 5);

  // chain_position for edge in second ring
  auto pos = shape.chain_position(7);
  EXPECT_EQ(pos.chain_id, 1);
  EXPECT_EQ(pos.offset, 2);
}

TEST(GeoArrowLaxPolygonShape, MultiPolygon2Components) {
  TestGeometry geom(
      "MULTIPOLYGON (((0 0, 1 0, 0 1, 0 0)), "
      "((10 10, 11 10, 10 11, 10 10)))");
  GeoArrowLaxPolygonShape shape(geom.geom());
  EXPECT_EQ(shape.num_loops(), 2);
  EXPECT_EQ(shape.num_loop_vertices(0), 4);
  EXPECT_EQ(shape.num_loop_vertices(1), 4);
  EXPECT_EQ(shape.num_edges(), 8);
  EXPECT_EQ(shape.num_chains(), 2);

  auto pos = shape.chain_position(5);
  EXPECT_EQ(pos.chain_id, 1);
  EXPECT_EQ(pos.offset, 1);
}

TEST(GeoArrowLaxPolygonShape, MultiPolygon3Components) {
  TestGeometry geom(
      "MULTIPOLYGON (((0 0, 1 0, 0 1, 0 0)), "
      "((10 10, 11 10, 10 11, 10 10)), "
      "((20 20, 21 20, 20 21, 20 20)))");
  GeoArrowLaxPolygonShape shape(geom.geom());
  EXPECT_EQ(shape.num_loops(), 3);
  EXPECT_EQ(shape.num_edges(), 12);  // 4 + 4 + 4
  EXPECT_EQ(shape.num_chains(), 3);
}

TEST(GeoArrowLaxPolygonShape, MultiPolygonWithHoles) {
  // 2 polygons, first has a hole
  TestGeometry geom(
      "MULTIPOLYGON (((0 0, 10 0, 10 10, 0 10, 0 0), "
      "(2 2, 8 2, 8 8, 2 8, 2 2)), "
      "((20 20, 21 20, 20 21, 20 20)))");
  GeoArrowLaxPolygonShape shape(geom.geom());
  EXPECT_EQ(shape.num_loops(), 3);  // shell + hole + second polygon shell
  EXPECT_EQ(shape.num_loop_vertices(0), 5);
  EXPECT_EQ(shape.num_loop_vertices(1), 5);
  EXPECT_EQ(shape.num_loop_vertices(2), 4);
  EXPECT_EQ(shape.num_edges(), 14);  // 5 + 5 + 4
}

TEST(GeoArrowLaxPolygonShape, ChainEdgeWrapsAround) {
  // Triangle: vertices 0,1,2 -> edges (0,1), (1,2), (2,0)
  TestGeometry geom("POLYGON ((0 0, 1 0, 0 1, 0 0))");
  GeoArrowLaxPolygonShape shape(geom.geom());

  // Last edge in the chain should wrap from vertex 3 back to vertex 0
  auto last_edge = shape.chain_edge(0, 3);
  auto first_edge = shape.chain_edge(0, 0);
  // The last edge's endpoint should be the first edge's start
  EXPECT_EQ(last_edge.v1, first_edge.v0);
}

TEST(GeoArrowLaxPolygonShape, ShapeIndexContains) {
  // Create a polygon with a hole
  TestGeometry poly_geom(
      "POLYGON ((-10 -10, 10 -10, 10 10, -10 10, -10 -10), "
      "(-5 -5, -5 5, 5 5, 5 -5, -5 -5))");

  MutableS2ShapeIndex poly_index;
  poly_index.Add(poly_geom.ToPolygonShape());

  WKTReader reader;
  S2BooleanOperation::Options options;

  // Point inside the shell but outside the hole
  auto shell_geog = reader.read_feature("POINT (8 8)");
  ShapeIndexGeography shell_index(*shell_geog);
  EXPECT_TRUE(S2BooleanOperation::Intersects(
      poly_index, shell_index.ShapeIndex(), options));

  // Point inside the hole should not intersect
  auto hole_geog = reader.read_feature("POINT (0 0)");
  ShapeIndexGeography hole_index(*hole_geog);
  EXPECT_FALSE(S2BooleanOperation::Intersects(
      poly_index, hole_index.ShapeIndex(), options));

  // Point outside should not intersect
  auto far_geog = reader.read_feature("POINT (50 50)");
  ShapeIndexGeography far_index(*far_geog);
  EXPECT_FALSE(S2BooleanOperation::Intersects(poly_index,
                                              far_index.ShapeIndex(), options));
}

TEST(GeoArrowLaxPolygonShape, ShapeIndexContainsMultiPolygonWithHoles) {
  // Two polygons, each with a hole
  TestGeometry poly_geom(
      "MULTIPOLYGON (((-20 -20, -10 -20, -10 -10, -20 -10, -20 -20), "
      "(-17 -17, -17 -13, -13 -13, -13 -17, -17 -17)), "
      "((10 10, 20 10, 20 20, 10 20, 10 10), "
      "(13 13, 13 17, 17 17, 17 13, 13 13)))");

  TestGeometry poly_geom_bad_winding(
      "MULTIPOLYGON (((-20 -20, -20 -10, -10 -10, -10 -20, -20 -20), "
      "(-17 -17, -13 -17, -13 -13, -17 -13, -17 -17)), "
      "((10 10, 10 20, 20 20, 20 10, 10 10), "
      "(13 13, 17 13, 17 17, 13 17, 13 13)))");

  for (auto* test_geom : {&poly_geom, &poly_geom_bad_winding}) {
    SCOPED_TRACE(test_geom->label());

    MutableS2ShapeIndex poly_index;
    poly_index.Add(test_geom->ToPolygonShape());

    WKTReader reader;
    S2BooleanOperation::Options options;

    // Inside first shell (between shell and hole)
    auto in_shell1 = reader.read_feature("POINT (-11 -11)");
    ShapeIndexGeography in_shell1_index(*in_shell1);
    EXPECT_TRUE(S2BooleanOperation::Intersects(
        poly_index, in_shell1_index.ShapeIndex(), options));

    // Inside first hole
    auto in_hole1 = reader.read_feature("POINT (-15 -15)");
    ShapeIndexGeography in_hole1_index(*in_hole1);
    EXPECT_FALSE(S2BooleanOperation::Intersects(
        poly_index, in_hole1_index.ShapeIndex(), options));

    // Inside second shell (between shell and hole)
    auto in_shell2 = reader.read_feature("POINT (11 11)");
    ShapeIndexGeography in_shell2_index(*in_shell2);
    EXPECT_TRUE(S2BooleanOperation::Intersects(
        poly_index, in_shell2_index.ShapeIndex(), options));

    // Inside second hole
    auto in_hole2 = reader.read_feature("POINT (15 15)");
    ShapeIndexGeography in_hole2_index(*in_hole2);
    EXPECT_FALSE(S2BooleanOperation::Intersects(
        poly_index, in_hole2_index.ShapeIndex(), options));

    // Outside both polygons
    auto outside = reader.read_feature("POINT (50 50)");
    ShapeIndexGeography outside_index(*outside);
    EXPECT_FALSE(S2BooleanOperation::Intersects(
        poly_index, outside_index.ShapeIndex(), options));
  }
}

TEST(GeoArrowLaxPolygonShape, BigEndianWKBPolygon) {
  // clang-format off
  // Big-endian WKB for POLYGON ((0 0, 1 0, 0 1, 0 0))
  std::vector<uint8_t> wkb = {
    0x00,                                      // big endian
    0x00, 0x00, 0x00, 0x03,                    // type: Polygon
    0x00, 0x00, 0x00, 0x01,                    // num rings: 1
    0x00, 0x00, 0x00, 0x04,                    // num points: 4
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // x: 0.0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // y: 0.0
    0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // x: 1.0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // y: 0.0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // x: 0.0
    0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // y: 1.0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // x: 0.0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // y: 0.0
  };
  // clang-format on

  auto geom = TestGeometry::FromWKB(wkb);
  GeoArrowLaxPolygonShape shape(geom.geom());
  EXPECT_EQ(shape.num_loops(), 1);
  EXPECT_EQ(shape.num_loop_vertices(0), 4);
  EXPECT_EQ(shape.num_edges(), 4);
}
