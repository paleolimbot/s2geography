#include "s2geography/geoarrow-geography.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "geoarrow/geoarrow.hpp"
#include "s2geography/geography.h"
#include "s2geography/wkt-reader.h"

using namespace s2geography;

/// \brief An owning wrapper around a GeoArrowGeometry with utilities to
/// construct from WKT or WKB
class TestGeometry {
 public:
  TestGeometry() : oriented_(false) {
    GEOARROW_THROW_NOT_OK(nullptr, GeoArrowGeometryInit(&geom_));
  }

  ~TestGeometry() { GeoArrowGeometryReset(&geom_); }

  TestGeometry(const TestGeometry&) = delete;
  TestGeometry& operator=(const TestGeometry&) = delete;

  TestGeometry(TestGeometry&& other) noexcept
      : geom_(other.geom_),
        label_(std::move(other.label_)),
        oriented_(other.oriented_) {
    GeoArrowGeometryInit(&other.geom_);
  }

  TestGeometry& operator=(TestGeometry&& other) noexcept {
    if (this != &other) {
      GeoArrowGeometryReset(&geom_);
      geom_ = other.geom_;
      GeoArrowGeometryInit(&other.geom_);
      label_ = std::move(other.label_);
      oriented_ = other.oriented_;
    }
    return *this;
  }

  static TestGeometry FromWKT(std::string_view wkt) {
    TestGeometry result;
    result.label_ = wkt;

    struct GeoArrowStringView wkt_view{wkt.data(),
                                       static_cast<int64_t>(wkt.size())};

    struct GeoArrowVisitor v{};
    GeoArrowGeometryInitVisitor(&result.geom_, &v);

    struct GeoArrowWKTReader reader;
    GEOARROW_THROW_NOT_OK(nullptr, GeoArrowWKTReaderInit(&reader));
    GeoArrowErrorCode code = GeoArrowWKTReaderVisit(&reader, wkt_view, &v);
    GeoArrowWKTReaderReset(&reader);
    if (code != GEOARROW_OK) {
      throw Exception("Invalid WKT");
    }

    return result;
  }

  static TestGeometry FromWKB(const std::vector<uint8_t>& wkb) {
    TestGeometry result;
    struct GeoArrowWKBReader reader;
    GEOARROW_THROW_NOT_OK(nullptr, GeoArrowWKBReaderInit(&reader));
    struct GeoArrowBufferView src{wkb.data(), static_cast<int64_t>(wkb.size())};
    struct GeoArrowGeometryView view;
    GeoArrowErrorCode code =
        GeoArrowWKBReaderRead(&reader, src, &view, nullptr);
    if (code != GEOARROW_OK) {
      GeoArrowWKBReaderReset(&reader);
      throw Exception("Invalid WKB");
    }

    // Copy the parsed geometry into our owned GeoArrowGeometry
    code = GeoArrowGeometryShallowCopy(view, &result.geom_);
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

 private:
  struct GeoArrowGeometry geom_;
  std::string label_;
  bool oriented_;
};

/// \brief Utility to sanity check an S2Shape, which has global edge ids
/// but also has edges organized into chains.
void ValidateShape(const S2Shape& shape) {
  int num_edges = shape.num_edges();
  int num_chains = shape.num_chains();

  // Sum of chain lengths must equal num_edges
  int edge_sum = 0;
  for (int c = 0; c < num_chains; ++c) {
    auto chain = shape.chain(c);
    EXPECT_EQ(chain.start, edge_sum) << "chain " << c;
    EXPECT_GE(chain.length, 0) << "chain " << c;
    edge_sum += chain.length;
  }
  EXPECT_EQ(edge_sum, num_edges);

  // For each edge, verify edge() == chain_edge() at the position given by
  // chain_position(), and that chain_position() is consistent with chain()
  for (int e = 0; e < num_edges; ++e) {
    auto pos = shape.chain_position(e);
    EXPECT_GE(pos.chain_id, 0) << "edge " << e;
    EXPECT_LT(pos.chain_id, num_chains) << "edge " << e;

    auto chain = shape.chain(pos.chain_id);
    EXPECT_GE(pos.offset, 0) << "edge " << e;
    EXPECT_LT(pos.offset, chain.length) << "edge " << e;
    EXPECT_EQ(chain.start + pos.offset, e) << "edge " << e;

    auto edge = shape.edge(e);
    auto ce = shape.chain_edge(pos.chain_id, pos.offset);
    EXPECT_EQ(edge.v0, ce.v0) << "edge " << e;
    EXPECT_EQ(edge.v1, ce.v1) << "edge " << e;
  }
}

// GeoArrowGeom / GeoArrowChain / GeoArrowLoop primitive tests

TEST(GeoArrowGeom, VisitPoint) {
  auto geom = TestGeometry::FromWKT("POINT (1 2)");
  GeoArrowGeom g(geom.geom());

  std::vector<S2Point> vertices;
  g.VisitVertices([&](const S2Point& p) {
    vertices.push_back(p);
    return true;
  });
  ASSERT_EQ(vertices.size(), 1);
  EXPECT_EQ(vertices[0], S2LatLng::FromDegrees(2, 1).ToPoint());
}

TEST(GeoArrowGeom, VisitVerticesLineString) {
  auto geom = TestGeometry::FromWKT("LINESTRING (0 0, 1 1, 2 2)");
  GeoArrowGeom g(geom.geom());

  std::vector<S2Point> vertices;
  g.VisitVertices([&](const S2Point& p) {
    vertices.push_back(p);
    return true;
  });
  ASSERT_EQ(vertices.size(), 3);
  EXPECT_EQ(vertices[0], S2LatLng::FromDegrees(0, 0).ToPoint());
  EXPECT_EQ(vertices[1], S2LatLng::FromDegrees(1, 1).ToPoint());
  EXPECT_EQ(vertices[2], S2LatLng::FromDegrees(2, 2).ToPoint());
}

TEST(GeoArrowGeom, VisitEdgesLineString) {
  auto geom = TestGeometry::FromWKT("LINESTRING (0 0, 1 1, 2 2)");
  GeoArrowGeom g(geom.geom());

  std::vector<S2Shape::Edge> edges;
  g.VisitEdges([&](const S2Shape::Edge& e) {
    edges.push_back(e);
    return true;
  });
  ASSERT_EQ(edges.size(), 2);
  EXPECT_EQ(edges[0].v0, S2LatLng::FromDegrees(0, 0).ToPoint());
  EXPECT_EQ(edges[0].v1, S2LatLng::FromDegrees(1, 1).ToPoint());
  EXPECT_EQ(edges[1].v0, S2LatLng::FromDegrees(1, 1).ToPoint());
  EXPECT_EQ(edges[1].v1, S2LatLng::FromDegrees(2, 2).ToPoint());
}

TEST(GeoArrowGeom, VisitEdgesSingleVertex) {
  auto geom = TestGeometry::FromWKT("POINT (5 10)");
  GeoArrowGeom g(geom.geom());

  int count = 0;
  g.VisitEdges([&](const S2Shape::Edge&) {
    ++count;
    return true;
  });
  EXPECT_EQ(count, 0);
}

TEST(GeoArrowGeom, VisitChainsMultiLineString) {
  auto geom =
      TestGeometry::FromWKT("MULTILINESTRING ((0 0, 1 1), (2 2, 3 3, 4 4))");

  GeoArrowGeom g(geom.geom());
  std::vector<uint32_t> chain_sizes;
  g.VisitChains([&](GeoArrowChain chain) {
    chain_sizes.push_back(chain.size());
    return true;
  });
  ASSERT_EQ(chain_sizes.size(), 2);
  EXPECT_EQ(chain_sizes[0], 2);
  EXPECT_EQ(chain_sizes[1], 3);
}

TEST(GeoArrowGeom, VisitChainsEmpty) {
  auto geom = TestGeometry::FromWKT("LINESTRING EMPTY");
  // An empty linestring still has one node with size 0
  GeoArrowGeom g(geom.geom());

  int count = 0;
  g.VisitChains([&](GeoArrowChain chain) {
    EXPECT_EQ(chain.size(), 0);
    ++count;
    return true;
  });
  EXPECT_EQ(count, 1);
}

TEST(GeoArrowGeom, VisitLoopsPolygon) {
  auto geom = TestGeometry::FromWKT("POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))");
  GeoArrowGeom g(geom.geom());

  std::vector<S2Point> scratch;
  int loop_count = 0;
  g.VisitLoops(&scratch, [&](GeoArrowLoop loop) {
    ++loop_count;
    EXPECT_EQ(loop.size(), 5);
    return true;
  });
  EXPECT_EQ(loop_count, 1);
}

TEST(GeoArrowGeom, VisitEdgesPolygon) {
  // Polygon ring with 5 coords (including closing vertex) / 4 edges
  auto geom = TestGeometry::FromWKT("POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))");
  GeoArrowGeom g(geom.geom());

  std::vector<S2Shape::Edge> edges;
  g.VisitEdges([&](const S2Shape::Edge& e) {
    edges.push_back(e);
    return true;
  });
  ASSERT_EQ(edges.size(), 4);

  // First edge: (0,0) / (10,0)
  EXPECT_EQ(edges[0].v0, S2LatLng::FromDegrees(0, 0).ToPoint());
  EXPECT_EQ(edges[0].v1, S2LatLng::FromDegrees(0, 10).ToPoint());
  // Last edge: (0,10) / (0,0)
  EXPECT_EQ(edges[3].v0, S2LatLng::FromDegrees(10, 0).ToPoint());
  EXPECT_EQ(edges[3].v1, S2LatLng::FromDegrees(0, 0).ToPoint());
}

TEST(GeoArrowGeom, DefaultConstructor) {
  GeoArrowGeom g;
  EXPECT_EQ(g.size(), 0);
  EXPECT_EQ(g.root(), nullptr);

  int count = 0;
  g.VisitVertices([&](S2Point) {
    ++count;
    return true;
  });
  EXPECT_EQ(count, 0);
}

TEST(GeoArrowGeom, VisitNativeVerticesPoint) {
  auto geom = TestGeometry::FromWKT("POINT ZM (1 2 3 4)");
  GeoArrowGeom g(geom.geom());

  std::vector<internal::GeoArrowVertex> vertices;
  g.VisitNativeVertices([&](const internal::GeoArrowVertex& v) {
    vertices.push_back(v);
    return true;
  });
  ASSERT_EQ(vertices.size(), 1);
  EXPECT_DOUBLE_EQ(vertices[0].lng, 1);
  EXPECT_DOUBLE_EQ(vertices[0].lat, 2);
  EXPECT_DOUBLE_EQ(vertices[0].zm[0], 3);
  EXPECT_DOUBLE_EQ(vertices[0].zm[1], 4);

  // Early termination
  int count = 0;
  EXPECT_FALSE(g.VisitNativeVertices([&](const internal::GeoArrowVertex&) {
    ++count;
    return false;
  }));
  EXPECT_EQ(count, 1);
}

TEST(GeoArrowGeom, VisitNativeVerticesLineString) {
  auto geom =
      TestGeometry::FromWKT("LINESTRING ZM (0 0 10 20, 1 1 11 21, 2 2 12 22)");
  GeoArrowGeom g(geom.geom());

  std::vector<internal::GeoArrowVertex> vertices;
  g.VisitNativeVertices([&](const internal::GeoArrowVertex& v) {
    vertices.push_back(v);
    return true;
  });
  ASSERT_EQ(vertices.size(), 3);
  EXPECT_DOUBLE_EQ(vertices[0].lng, 0);
  EXPECT_DOUBLE_EQ(vertices[0].lat, 0);
  EXPECT_DOUBLE_EQ(vertices[0].zm[0], 10);
  EXPECT_DOUBLE_EQ(vertices[0].zm[1], 20);
  EXPECT_DOUBLE_EQ(vertices[1].lng, 1);
  EXPECT_DOUBLE_EQ(vertices[1].lat, 1);
  EXPECT_DOUBLE_EQ(vertices[1].zm[0], 11);
  EXPECT_DOUBLE_EQ(vertices[1].zm[1], 21);
  EXPECT_DOUBLE_EQ(vertices[2].lng, 2);
  EXPECT_DOUBLE_EQ(vertices[2].lat, 2);
  EXPECT_DOUBLE_EQ(vertices[2].zm[0], 12);
  EXPECT_DOUBLE_EQ(vertices[2].zm[1], 22);

  // Early termination: stop after first vertex
  int count = 0;
  EXPECT_FALSE(g.VisitNativeVertices([&](const internal::GeoArrowVertex&) {
    ++count;
    return false;
  }));
  EXPECT_EQ(count, 1);
}

TEST(GeoArrowGeom, VisitNativeEdgesLineString) {
  auto geom =
      TestGeometry::FromWKT("LINESTRING ZM (0 0 10 20, 1 1 11 21, 2 2 12 22)");
  GeoArrowGeom g(geom.geom());

  std::vector<internal::GeoArrowEdge> edges;
  g.VisitNativeEdges([&](const internal::GeoArrowEdge& e) {
    edges.push_back(e);
    return true;
  });
  ASSERT_EQ(edges.size(), 2);
  EXPECT_DOUBLE_EQ(edges[0].v0.lng, 0);
  EXPECT_DOUBLE_EQ(edges[0].v0.lat, 0);
  EXPECT_DOUBLE_EQ(edges[0].v0.zm[0], 10);
  EXPECT_DOUBLE_EQ(edges[0].v0.zm[1], 20);
  EXPECT_DOUBLE_EQ(edges[0].v1.lng, 1);
  EXPECT_DOUBLE_EQ(edges[0].v1.lat, 1);
  EXPECT_DOUBLE_EQ(edges[0].v1.zm[0], 11);
  EXPECT_DOUBLE_EQ(edges[0].v1.zm[1], 21);
  EXPECT_DOUBLE_EQ(edges[1].v0.lng, 1);
  EXPECT_DOUBLE_EQ(edges[1].v0.lat, 1);
  EXPECT_DOUBLE_EQ(edges[1].v0.zm[0], 11);
  EXPECT_DOUBLE_EQ(edges[1].v0.zm[1], 21);
  EXPECT_DOUBLE_EQ(edges[1].v1.lng, 2);
  EXPECT_DOUBLE_EQ(edges[1].v1.lat, 2);
  EXPECT_DOUBLE_EQ(edges[1].v1.zm[0], 12);
  EXPECT_DOUBLE_EQ(edges[1].v1.zm[1], 22);

  // Early termination: stop after first edge
  int count = 0;
  EXPECT_FALSE(g.VisitNativeEdges([&](const internal::GeoArrowEdge&) {
    ++count;
    return false;
  }));
  EXPECT_EQ(count, 1);
}

TEST(GeoArrowGeom, VisitNativeEdgesSingleVertex) {
  auto geom = TestGeometry::FromWKT("POINT ZM (5 10 15 20)");
  GeoArrowGeom g(geom.geom());

  int count = 0;
  g.VisitNativeEdges([&](const internal::GeoArrowEdge&) {
    ++count;
    return true;
  });
  EXPECT_EQ(count, 0);
}

TEST(GeoArrowGeom, VisitNativeEdgesPolygon) {
  auto geom = TestGeometry::FromWKT(
      "POLYGON ZM ((0 0 1 2, 10 0 3 4, 10 10 5 6, 0 10 7 8, 0 0 1 2))");
  GeoArrowGeom g(geom.geom());

  std::vector<internal::GeoArrowEdge> edges;
  g.VisitNativeEdges([&](const internal::GeoArrowEdge& e) {
    edges.push_back(e);
    return true;
  });
  ASSERT_EQ(edges.size(), 4);

  EXPECT_DOUBLE_EQ(edges[0].v0.lng, 0);
  EXPECT_DOUBLE_EQ(edges[0].v0.lat, 0);
  EXPECT_DOUBLE_EQ(edges[0].v0.zm[0], 1);
  EXPECT_DOUBLE_EQ(edges[0].v0.zm[1], 2);
  EXPECT_DOUBLE_EQ(edges[0].v1.lng, 10);
  EXPECT_DOUBLE_EQ(edges[0].v1.lat, 0);
  EXPECT_DOUBLE_EQ(edges[0].v1.zm[0], 3);
  EXPECT_DOUBLE_EQ(edges[0].v1.zm[1], 4);
  EXPECT_DOUBLE_EQ(edges[3].v0.lng, 0);
  EXPECT_DOUBLE_EQ(edges[3].v0.lat, 10);
  EXPECT_DOUBLE_EQ(edges[3].v0.zm[0], 7);
  EXPECT_DOUBLE_EQ(edges[3].v0.zm[1], 8);
  EXPECT_DOUBLE_EQ(edges[3].v1.lng, 0);
  EXPECT_DOUBLE_EQ(edges[3].v1.lat, 0);
  EXPECT_DOUBLE_EQ(edges[3].v1.zm[0], 1);
  EXPECT_DOUBLE_EQ(edges[3].v1.zm[1], 2);

  // Early termination: stop after first edge
  int count = 0;
  EXPECT_FALSE(g.VisitNativeEdges([&](const internal::GeoArrowEdge&) {
    ++count;
    return false;
  }));
  EXPECT_EQ(count, 1);
}

TEST(GeoArrowChain, VertexAndEdge) {
  auto geom = TestGeometry::FromWKT("LINESTRING (3 4, 5 6, 7 8)");
  GeoArrowChain chain(geom.geom().root);

  auto v = chain.vertex(1);
  EXPECT_EQ(v, S2LatLng::FromDegrees(6, 5).ToPoint());

  auto e = chain.edge(0);
  EXPECT_EQ(e.v0, S2LatLng::FromDegrees(4, 3).ToPoint());
  EXPECT_EQ(e.v1, S2LatLng::FromDegrees(6, 5).ToPoint());

  e = chain.edge(1);
  EXPECT_EQ(e.v0, S2LatLng::FromDegrees(6, 5).ToPoint());
  EXPECT_EQ(e.v1, S2LatLng::FromDegrees(8, 7).ToPoint());
}

TEST(GeoArrowChain, NativeVertexAndEdge) {
  auto geom = TestGeometry::FromWKT(
      "LINESTRING ZM (3 4 100 200, 5 6 101 201, 7 8 102 202)");
  GeoArrowChain chain(geom.geom().root);

  auto v = chain.native_vertex(1);
  EXPECT_DOUBLE_EQ(v.lng, 5);
  EXPECT_DOUBLE_EQ(v.lat, 6);
  EXPECT_DOUBLE_EQ(v.zm[0], 101);
  EXPECT_DOUBLE_EQ(v.zm[1], 201);

  auto e = chain.native_edge(0);
  EXPECT_DOUBLE_EQ(e.v0.lng, 3);
  EXPECT_DOUBLE_EQ(e.v0.lat, 4);
  EXPECT_DOUBLE_EQ(e.v0.zm[0], 100);
  EXPECT_DOUBLE_EQ(e.v0.zm[1], 200);
  EXPECT_DOUBLE_EQ(e.v1.lng, 5);
  EXPECT_DOUBLE_EQ(e.v1.lat, 6);
  EXPECT_DOUBLE_EQ(e.v1.zm[0], 101);
  EXPECT_DOUBLE_EQ(e.v1.zm[1], 201);

  e = chain.native_edge(1);
  EXPECT_DOUBLE_EQ(e.v0.lng, 5);
  EXPECT_DOUBLE_EQ(e.v0.lat, 6);
  EXPECT_DOUBLE_EQ(e.v0.zm[0], 101);
  EXPECT_DOUBLE_EQ(e.v0.zm[1], 201);
  EXPECT_DOUBLE_EQ(e.v1.lng, 7);
  EXPECT_DOUBLE_EQ(e.v1.lat, 8);
  EXPECT_DOUBLE_EQ(e.v1.zm[0], 102);
  EXPECT_DOUBLE_EQ(e.v1.zm[1], 202);
}

// Specifically check loop functions

TEST(GeoArrowLoop, LoopMetrics) {
  std::vector<S2Point> scratch;

  // Check a positive reference and a negative reference
  S2Shape::ReferencePoint reference_out{S2LatLng::FromDegrees(-1, -1).ToPoint(),
                                        false};
  S2Shape::ReferencePoint reference_in{S2LatLng::FromDegrees(2, 2).ToPoint(),
                                       true};

  // Counterclockwise wound shell
  auto shell = TestGeometry::FromWKT("LINESTRING (0 0, 10 0, 0 10, 0 0)");
  GeoArrowLoop shell_loop(shell.geom().root, &scratch);
  ASSERT_GT(shell_loop.GetSignedArea(), 0);
  ASSERT_GT(shell_loop.GetCurvature(), 0);
  S2Point centroid = shell_loop.GetCentroid();
  ASSERT_NEAR(centroid.Norm(), std::abs(shell_loop.GetSignedArea()), 1e-4);

  // Containment
  EXPECT_TRUE(shell_loop.BruteForceContains(
      S2LatLng::FromDegrees(1.1, 1.2).ToPoint(), reference_in));
  EXPECT_FALSE(shell_loop.BruteForceContains(
      S2LatLng::FromDegrees(10, 10).ToPoint(), reference_in));
  EXPECT_TRUE(shell_loop.BruteForceContains(
      S2LatLng::FromDegrees(1.1, 1.2).ToPoint(), reference_out));
  EXPECT_FALSE(shell_loop.BruteForceContains(
      S2LatLng::FromDegrees(10, 10).ToPoint(), reference_out));

  // Clockwise wound hole
  auto hole = TestGeometry::FromWKT("LINESTRING (1 1, 1 5, 5 1, 1 1)");
  GeoArrowLoop hole_loop(hole.geom().root, &scratch);
  ASSERT_LT(hole_loop.GetSignedArea(), 0);
  ASSERT_LT(hole_loop.GetCurvature(), 0);
  centroid = hole_loop.GetCentroid();

  // Containment
  ASSERT_NEAR(centroid.Norm(), std::abs(hole_loop.GetSignedArea()), 1e-4);
  EXPECT_TRUE(hole_loop.BruteForceContains(
      S2LatLng::FromDegrees(1.1, 1.2).ToPoint(), reference_in));
  EXPECT_FALSE(hole_loop.BruteForceContains(
      S2LatLng::FromDegrees(5, 5).ToPoint(), reference_in));
  EXPECT_TRUE(hole_loop.BruteForceContains(
      S2LatLng::FromDegrees(1.1, 1.2).ToPoint(), reference_out));
  EXPECT_FALSE(hole_loop.BruteForceContains(
      S2LatLng::FromDegrees(5, 5).ToPoint(), reference_out));
}

// Shape tests

TEST(GeoArrowPointShape, DefaultConstructor) {
  GeoArrowPointShape shape;
  EXPECT_EQ(shape.num_edges(), 0);
  EXPECT_EQ(shape.dimension(), 0);
  EXPECT_EQ(shape.num_chains(), 0);
  EXPECT_EQ(shape.num_vertices(), 0);
  ValidateShape(shape);
}

TEST(GeoArrowPointShape, EmptyPoint) {
  auto geom = TestGeometry::FromWKT("POINT EMPTY");
  GeoArrowPointShape shape(geom.geom());
  EXPECT_EQ(shape.num_edges(), 0);
  EXPECT_EQ(shape.dimension(), 0);
  EXPECT_EQ(shape.num_chains(), 0);
  EXPECT_EQ(shape.num_vertices(), 0);
  ValidateShape(shape);
}

TEST(GeoArrowPointShape, EmptyMultiPoint) {
  auto geom = TestGeometry::FromWKT("MULTIPOINT EMPTY");
  GeoArrowPointShape shape(geom.geom());
  EXPECT_EQ(shape.num_edges(), 0);
  EXPECT_EQ(shape.dimension(), 0);
  EXPECT_EQ(shape.num_chains(), 0);
  EXPECT_EQ(shape.num_vertices(), 0);
  ValidateShape(shape);
}

TEST(GeoArrowPointShape, MultiPointWithEmpty) {
  auto geom = TestGeometry::FromWKT("MULTIPOINT ((0 1), EMPTY)");
  EXPECT_THROW(GeoArrowPointShape shape(geom.geom()), Exception);
}

TEST(GeoArrowPointShape, SinglePoint) {
  auto geom = TestGeometry::FromWKT("POINT (30 10)");
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

  ValidateShape(shape);
}

TEST(GeoArrowPointShape, MultiPoint) {
  auto geom = TestGeometry::FromWKT("MULTIPOINT ((0 0), (1 1), (2 2))");
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

  ValidateShape(shape);
}

TEST(GeoArrowPointShape, BigEndianWKB) {
  // clang-format off
  // Big-endian WKB for POINT ZM (30 10 5 15)
  std::vector<uint8_t> wkb = {
    0x00,                                      // big endian
    0x00, 0x00, 0x0b, 0xb9,                    // type: Point ZM (3001)
    0x40, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // x: 30.0
    0x40, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // y: 10.0
    0x40, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // z: 5.0
    0x40, 0x2e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // m: 15.0
  };
  // clang-format on

  auto geom = TestGeometry::FromWKB(wkb);
  GeoArrowPointShape shape(geom.geom());
  EXPECT_EQ(shape.num_vertices(), 1);
  EXPECT_EQ(shape.num_edges(), 1);
  EXPECT_EQ(shape.num_chains(), 1);

  // The point's degenerate edge should have correct native coordinates
  auto e = shape.native_edge(0);
  EXPECT_DOUBLE_EQ(e.v0.lng, 30);
  EXPECT_DOUBLE_EQ(e.v0.lat, 10);
  EXPECT_DOUBLE_EQ(e.v0.zm[0], 5);
  EXPECT_DOUBLE_EQ(e.v0.zm[1], 15);
  EXPECT_DOUBLE_EQ(e.v1.lng, 30);
  EXPECT_DOUBLE_EQ(e.v1.lat, 10);
  EXPECT_DOUBLE_EQ(e.v1.zm[0], 5);
  EXPECT_DOUBLE_EQ(e.v1.zm[1], 15);

  ValidateShape(shape);
}

TEST(GeoArrowPointShape, ShapeIndexIntersection) {
  auto point_geom = TestGeometry::FromWKT("MULTIPOINT ((0 0), (1 1), (50 50))");
  GeoArrowPointShape shape(point_geom.geom());
  EXPECT_EQ(shape.num_chains(), 1);
  ValidateShape(shape);

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
  ValidateShape(shape);
}

TEST(GeoArrowLaxPolylineShape, EmptyLinestring) {
  auto geom = TestGeometry::FromWKT("LINESTRING EMPTY");
  GeoArrowLaxPolylineShape shape(geom.geom());
  EXPECT_EQ(shape.num_edges(), 0);
  EXPECT_EQ(shape.dimension(), 1);
  EXPECT_EQ(shape.num_chains(), 0);
  ValidateShape(shape);
}

TEST(GeoArrowLaxPolylineShape, EmptyMultiLinestring) {
  auto geom = TestGeometry::FromWKT("MULTILINESTRING EMPTY");
  GeoArrowLaxPolylineShape shape(geom.geom());
  EXPECT_EQ(shape.num_edges(), 0);
  EXPECT_EQ(shape.dimension(), 1);
  EXPECT_EQ(shape.num_chains(), 0);
  ValidateShape(shape);
}

TEST(GeoArrowLaxPolylineShape, Linestring) {
  auto geom = TestGeometry::FromWKT("LINESTRING (0 0, 0 1, 1 0)");
  GeoArrowLaxPolylineShape shape(geom.geom());
  EXPECT_EQ(shape.num_edges(), 2);
  EXPECT_EQ(shape.dimension(), 1);
  EXPECT_EQ(shape.num_chains(), 1);
  ValidateShape(shape);
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
  ValidateShape(shape);
}

TEST(GeoArrowLaxPolylineShape, MultiLinestring2Components) {
  auto geom = TestGeometry::FromWKT(
      "MULTILINESTRING ((0 0, 1 1, 2 2), (10 10, 11 11))");
  GeoArrowLaxPolylineShape shape(geom.geom());
  EXPECT_EQ(shape.num_chains(), 2);
  EXPECT_EQ(shape.num_edges(), 3);  // 2 + 1

  // chain_position maps global edge ids to (chain, offset)
  auto pos0 = shape.chain_position(0);
  EXPECT_EQ(pos0.chain_id, 0);
  EXPECT_EQ(pos0.offset, 0);

  auto pos2 = shape.chain_position(2);
  EXPECT_EQ(pos2.chain_id, 1);
  EXPECT_EQ(pos2.offset, 0);

  ValidateShape(shape);
}

TEST(GeoArrowLaxPolylineShape, MultiLinestring3Components) {
  auto geom = TestGeometry::FromWKT(
      "MULTILINESTRING ((0 0, 1 0), (2 0, 3 0, 4 0), (5 0, 6 0))");
  GeoArrowLaxPolylineShape shape(geom.geom());
  EXPECT_EQ(shape.num_chains(), 3);
  EXPECT_EQ(shape.num_edges(), 4);  // 1 + 2 + 1

  auto pos3 = shape.chain_position(3);
  EXPECT_EQ(pos3.chain_id, 2);
  EXPECT_EQ(pos3.offset, 0);

  ValidateShape(shape);
}

TEST(GeoArrowLaxPolylineShape, MultiLinestring4Components) {
  auto geom = TestGeometry::FromWKT(
      "MULTILINESTRING ((0 0, 1 0), (2 0, 3 0), (4 0, 5 0), (6 0, 7 0))");
  GeoArrowLaxPolylineShape shape(geom.geom());
  EXPECT_EQ(shape.num_chains(), 4);
  EXPECT_EQ(shape.num_edges(), 4);

  auto pos0 = shape.chain_position(0);
  EXPECT_EQ(pos0.chain_id, 0);
  EXPECT_EQ(pos0.offset, 0);

  auto pos3 = shape.chain_position(3);
  EXPECT_EQ(pos3.chain_id, 3);
  EXPECT_EQ(pos3.offset, 0);

  ValidateShape(shape);
}

TEST(GeoArrowLaxPolylineShape, MultiLinestring3ComponentsOneEmpty) {
  auto geom = TestGeometry::FromWKT(
      "MULTILINESTRING ((0 0, 1 0, 2 0), EMPTY, (3 0, 4 0))");
  GeoArrowLaxPolylineShape shape(geom.geom());
  EXPECT_EQ(shape.num_chains(), 3);
  // The EMPTY linestring contributes 0 vertices and 0 edges, so the total
  // edge count should be (3 - 1) + 0 + (2 - 1) = 2 + 0 + 1 = 3.
  EXPECT_EQ(shape.num_edges(), 3);

  ValidateShape(shape);
}

TEST(GeoArrowLaxPolylineShape, ShapeIndexIntersection) {
  // Create a multilinestring with 4 components that cross over a region
  auto line_geom = TestGeometry::FromWKT(
      "MULTILINESTRING ((-1 0, 1 0), (0 -1, 0 1), "
      "(-1 -1, 1 1), (-1 1, 1 -1))");
  GeoArrowLaxPolylineShape shape(line_geom.geom());
  EXPECT_EQ(shape.num_chains(), 4);
  ValidateShape(shape);

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
  ValidateShape(shape);
}

TEST(GeoArrowLaxPolygonShape, EmptyPolygon) {
  auto geom = TestGeometry::FromWKT("POLYGON EMPTY");
  GeoArrowLaxPolygonShape shape(geom.geom());
  EXPECT_EQ(shape.num_edges(), 0);
  EXPECT_EQ(shape.dimension(), 2);
  EXPECT_EQ(shape.num_chains(), 0);
  EXPECT_FALSE(shape.BruteForceContains(S2LatLng::FromDegrees(0, 0).ToPoint()));
  ValidateShape(shape);
}

TEST(GeoArrowLaxPolygonShape, EmptyMultiPolygon) {
  auto geom = TestGeometry::FromWKT("MULTIPOLYGON EMPTY");
  GeoArrowLaxPolygonShape shape(geom.geom());
  EXPECT_EQ(shape.num_edges(), 0);
  EXPECT_EQ(shape.dimension(), 2);
  EXPECT_EQ(shape.num_chains(), 0);
  EXPECT_FALSE(shape.BruteForceContains(S2LatLng::FromDegrees(0, 0).ToPoint()));
  ValidateShape(shape);
}

TEST(GeoArrowLaxPolygonShape, SimpleTriangle) {
  auto geom = TestGeometry::FromWKT("POLYGON ((0 0, 1 0, 0 1, 0 0))");
  GeoArrowLaxPolygonShape shape(geom.geom());
  EXPECT_EQ(shape.chain(0).length, 3);
  // num_edges == total vertices (each ring is closed)
  EXPECT_EQ(shape.num_edges(), 3);
  EXPECT_EQ(shape.num_chains(), 1);
  EXPECT_EQ(shape.dimension(), 2);

  auto chain0 = shape.chain(0);
  EXPECT_EQ(chain0.start, 0);
  EXPECT_EQ(chain0.length, 3);

  // Check brute force containment outside and inside using the computed
  // reference
  EXPECT_FALSE(
      shape.BruteForceContains(S2LatLng::FromDegrees(-1, 1).ToPoint()));
  EXPECT_TRUE(
      shape.BruteForceContains(S2LatLng::FromDegrees(0.1, 0.1).ToPoint()));

  // Check with custom reference points. This is to ensure that the containment
  // logic for multiple loops works whether the reference point is inside or
  // outside.
  S2Shape::ReferencePoint reference_out{S2LatLng::FromDegrees(-1, -1).ToPoint(),
                                        false};
  S2Shape::ReferencePoint reference_in{
      S2LatLng::FromDegrees(0.2, 0.2).ToPoint(), true};

  EXPECT_FALSE(shape.BruteForceContains(S2LatLng::FromDegrees(-1, 1).ToPoint(),
                                        reference_in));
  EXPECT_TRUE(shape.BruteForceContains(
      S2LatLng::FromDegrees(0.1, 0.1).ToPoint(), reference_in));
  EXPECT_FALSE(shape.BruteForceContains(S2LatLng::FromDegrees(-1, 1).ToPoint(),
                                        reference_out));
  EXPECT_TRUE(shape.BruteForceContains(
      S2LatLng::FromDegrees(0.1, 0.1).ToPoint(), reference_out));

  // Check loops for hole status
  std::vector<S2Point> scratch;
  std::vector<bool> hole_flags;
  shape.geom().VisitLoops(&scratch, [&](GeoArrowLoop loop) {
    hole_flags.push_back(loop.is_hole());
    return true;
  });
  ASSERT_EQ(hole_flags.size(), 1);
  EXPECT_FALSE(hole_flags[0]);  // shell

  ValidateShape(shape);
}

TEST(GeoArrowLaxPolygonShape, PolygonWithHole) {
  auto geom = TestGeometry::FromWKT(
      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0), "
      "(2 2, 8 2, 8 8, 2 8, 2 2))");
  GeoArrowLaxPolygonShape shape(geom.geom());
  EXPECT_EQ(shape.chain(0).length, 4);
  EXPECT_EQ(shape.chain(1).length, 4);
  EXPECT_EQ(shape.num_edges(), 8);
  EXPECT_EQ(shape.num_chains(), 2);

  auto chain0 = shape.chain(0);
  EXPECT_EQ(chain0.start, 0);
  EXPECT_EQ(chain0.length, 4);

  auto chain1 = shape.chain(1);
  EXPECT_EQ(chain1.start, 4);
  EXPECT_EQ(chain1.length, 4);

  // chain_position for edge in second ring
  auto pos = shape.chain_position(6);
  EXPECT_EQ(pos.chain_id, 1);
  EXPECT_EQ(pos.offset, 2);

  // Check brute force containment outside and inside using the computed
  // reference (including a point inside the hole)
  EXPECT_FALSE(
      shape.BruteForceContains(S2LatLng::FromDegrees(-1, 1).ToPoint()));
  EXPECT_TRUE(shape.BruteForceContains(S2LatLng::FromDegrees(1, 1).ToPoint()));
  EXPECT_FALSE(shape.BruteForceContains(S2LatLng::FromDegrees(3, 3).ToPoint()));

  // Check with custom reference points. This is to ensure that the containment
  // logic for multiple loops works whether the reference point is inside or
  // outside.
  S2Shape::ReferencePoint reference_out{S2LatLng::FromDegrees(-1, -1).ToPoint(),
                                        false};
  S2Shape::ReferencePoint reference_in{
      S2LatLng::FromDegrees(0.2, 0.2).ToPoint(), true};

  EXPECT_FALSE(shape.BruteForceContains(S2LatLng::FromDegrees(-1, 1).ToPoint(),
                                        reference_in));
  EXPECT_TRUE(shape.BruteForceContains(S2LatLng::FromDegrees(1, 1).ToPoint(),
                                       reference_in));
  EXPECT_FALSE(shape.BruteForceContains(S2LatLng::FromDegrees(3, 3).ToPoint(),
                                        reference_in));

  EXPECT_FALSE(shape.BruteForceContains(S2LatLng::FromDegrees(-1, 1).ToPoint(),
                                        reference_out));
  EXPECT_TRUE(shape.BruteForceContains(S2LatLng::FromDegrees(1, 1).ToPoint(),
                                       reference_out));
  EXPECT_FALSE(shape.BruteForceContains(S2LatLng::FromDegrees(3, 3).ToPoint(),
                                        reference_out));

  // Check loops for hole status
  std::vector<S2Point> scratch;
  std::vector<bool> hole_flags;
  shape.geom().VisitLoops(&scratch, [&](GeoArrowLoop loop) {
    hole_flags.push_back(loop.is_hole());
    return true;
  });
  ASSERT_EQ(hole_flags.size(), 2);
  EXPECT_FALSE(hole_flags[0]);  // shell
  EXPECT_TRUE(hole_flags[1]);   // hole

  ValidateShape(shape);
}

TEST(GeoArrowLaxPolygonShape, MultiPolygon2Components) {
  auto geom = TestGeometry::FromWKT(
      "MULTIPOLYGON (((0 0, 1 0, 0 1, 0 0)), "
      "((10 10, 11 10, 10 11, 10 10)))");
  GeoArrowLaxPolygonShape shape(geom.geom());
  EXPECT_EQ(shape.chain(0).length, 3);
  EXPECT_EQ(shape.chain(1).length, 3);
  EXPECT_EQ(shape.num_edges(), 6);
  EXPECT_EQ(shape.num_chains(), 2);

  auto pos = shape.chain_position(5);
  EXPECT_EQ(pos.chain_id, 1);
  EXPECT_EQ(pos.offset, 2);

  // Check loops for hole status
  std::vector<S2Point> scratch;
  std::vector<bool> hole_flags;
  shape.geom().VisitLoops(&scratch, [&](GeoArrowLoop loop) {
    hole_flags.push_back(loop.is_hole());
    return true;
  });
  ASSERT_EQ(hole_flags.size(), 2);
  EXPECT_FALSE(hole_flags[0]);  // shell
  EXPECT_FALSE(hole_flags[1]);  // shell

  ValidateShape(shape);
}

TEST(GeoArrowLaxPolygonShape, MultiPolygon3Components) {
  auto geom = TestGeometry::FromWKT(
      "MULTIPOLYGON (((0 0, 1 0, 0 1, 0 0)), "
      "((10 10, 11 10, 10 11, 10 10)), "
      "((20 20, 21 20, 20 21, 20 20)))");
  GeoArrowLaxPolygonShape shape(geom.geom());
  EXPECT_EQ(shape.num_edges(), 9);  // 3 + 3 + 3
  EXPECT_EQ(shape.num_chains(), 3);

  // Check loops for hole status
  std::vector<S2Point> scratch;
  std::vector<bool> hole_flags;
  shape.geom().VisitLoops(&scratch, [&](GeoArrowLoop loop) {
    hole_flags.push_back(loop.is_hole());
    return true;
  });
  ASSERT_EQ(hole_flags.size(), 3);
  EXPECT_FALSE(hole_flags[0]);  // shell
  EXPECT_FALSE(hole_flags[1]);  // shell
  EXPECT_FALSE(hole_flags[2]);  // shell

  ValidateShape(shape);
}

TEST(GeoArrowLaxPolygonShape, MultiPolygonWithHoles) {
  // 2 polygons, first has a hole
  auto geom = TestGeometry::FromWKT(
      "MULTIPOLYGON (((0 0, 10 0, 10 10, 0 10, 0 0), "
      "(2 2, 8 2, 8 8, 2 8, 2 2)), "
      "((20 20, 21 20, 20 21, 20 20)))");
  GeoArrowLaxPolygonShape shape(geom.geom());
  EXPECT_EQ(shape.num_chains(), 3);  // shell + hole + second polygon shell
  EXPECT_EQ(shape.chain(0).length, 4);
  EXPECT_EQ(shape.chain(1).length, 4);
  EXPECT_EQ(shape.chain(2).length, 3);
  EXPECT_EQ(shape.num_edges(), 11);  // 4 + 4 + 3

  // Check brute force containment outside, inside shell, inside hole, and
  // inside the second polygon
  EXPECT_FALSE(
      shape.BruteForceContains(S2LatLng::FromDegrees(-1, 1).ToPoint()));
  EXPECT_TRUE(shape.BruteForceContains(S2LatLng::FromDegrees(1, 1).ToPoint()));
  EXPECT_FALSE(shape.BruteForceContains(S2LatLng::FromDegrees(3, 3).ToPoint()));
  EXPECT_TRUE(
      shape.BruteForceContains(S2LatLng::FromDegrees(20.1, 20.1).ToPoint()));

  // Check with custom reference points
  S2Shape::ReferencePoint reference_out{S2LatLng::FromDegrees(-1, -1).ToPoint(),
                                        false};
  S2Shape::ReferencePoint reference_in{
      S2LatLng::FromDegrees(0.2, 0.2).ToPoint(), true};

  EXPECT_FALSE(shape.BruteForceContains(S2LatLng::FromDegrees(-1, 1).ToPoint(),
                                        reference_in));
  EXPECT_TRUE(shape.BruteForceContains(S2LatLng::FromDegrees(1, 1).ToPoint(),
                                       reference_in));
  EXPECT_FALSE(shape.BruteForceContains(S2LatLng::FromDegrees(3, 3).ToPoint(),
                                        reference_in));
  EXPECT_TRUE(shape.BruteForceContains(
      S2LatLng::FromDegrees(20.1, 20.1).ToPoint(), reference_in));

  EXPECT_FALSE(shape.BruteForceContains(S2LatLng::FromDegrees(-1, 1).ToPoint(),
                                        reference_out));
  EXPECT_TRUE(shape.BruteForceContains(S2LatLng::FromDegrees(1, 1).ToPoint(),
                                       reference_out));
  EXPECT_FALSE(shape.BruteForceContains(S2LatLng::FromDegrees(3, 3).ToPoint(),
                                        reference_out));
  EXPECT_TRUE(shape.BruteForceContains(
      S2LatLng::FromDegrees(20.1, 20.1).ToPoint(), reference_out));

  // Check loops for hole status
  std::vector<S2Point> scratch;
  std::vector<bool> hole_flags;
  shape.geom().VisitLoops(&scratch, [&](GeoArrowLoop loop) {
    hole_flags.push_back(loop.is_hole());
    return true;
  });
  ASSERT_EQ(hole_flags.size(), 3);
  EXPECT_FALSE(hole_flags[0]);  // shell
  EXPECT_TRUE(hole_flags[1]);   // hole
  EXPECT_FALSE(hole_flags[2]);  // shell

  ValidateShape(shape);
}

TEST(GeoArrowLaxPolygonShape, ChainEdgeWrapsAround) {
  // Triangle: vertices 0,1,2 -> edges (0,1), (1,2), (2,0)
  auto geom = TestGeometry::FromWKT("POLYGON ((0 0, 1 0, 0 1, 0 0))");
  GeoArrowLaxPolygonShape shape(geom.geom());

  // Last edge in the chain should wrap from vertex 3 back to vertex 0
  auto last_edge = shape.chain_edge(0, 2);
  auto first_edge = shape.chain_edge(0, 0);
  // The last edge's endpoint should be the first edge's start
  EXPECT_EQ(last_edge.v1, first_edge.v0);
}

TEST(GeoArrowLaxPolygonShape, ShapeIndexContains) {
  // Create a polygon with a hole
  auto poly_geom = TestGeometry::FromWKT(
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
  auto poly_geom = TestGeometry::FromWKT(
      "MULTIPOLYGON (((-20 -20, -10 -20, -10 -10, -20 -10, -20 -20), "
      "(-17 -17, -17 -13, -13 -13, -13 -17, -17 -17)), "
      "((10 10, 20 10, 20 20, 10 20, 10 10), "
      "(13 13, 13 17, 17 17, 17 13, 13 13)))");

  auto poly_geom_bad_winding = TestGeometry::FromWKT(
      "MULTIPOLYGON (((-20 -20, -20 -10, -10 -10, -10 -20, -20 -20), "
      "(-17 -17, -13 -17, -13 -13, -17 -13, -17 -17)), "
      "((10 10, 10 20, 20 20, 20 10, 10 10), "
      "(13 13, 17 13, 17 17, 13 17, 13 13)))");

  for (auto* test_geom : {&poly_geom, &poly_geom_bad_winding}) {
    SCOPED_TRACE(test_geom->label());

    MutableS2ShapeIndex poly_index;
    auto shape = test_geom->ToPolygonShape();
    ValidateShape(*shape);
    poly_index.Add(std::move(shape));

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
  EXPECT_EQ(shape.num_chains(), 1);
  EXPECT_EQ(shape.chain(0).length, 3);
  EXPECT_EQ(shape.num_edges(), 3);

  ValidateShape(shape);
}

// --- GeoArrowGeography tests ---

class GeoArrowGeographyTest : public ::testing::Test {
 protected:
  /// Helper: create a GeoArrowGeography from WKT backed by an owning
  /// TestGeometry (stored in geoms_ so the memory outlives the geography).
  GeoArrowGeography MakeGeography(std::string_view wkt, bool oriented = false) {
    geoms_.push_back(TestGeometry::FromWKT(wkt));
    GeoArrowGeography geog;

    if (oriented) {
      geog.InitOriented(geoms_.back().geom());
    } else {
      geog.Init(geoms_.back().geom());
    }

    return geog;
  }

  std::vector<TestGeometry> geoms_;
};

TEST_F(GeoArrowGeographyTest, DefaultConstructor) {
  GeoArrowGeography geog;
  EXPECT_TRUE(geog.is_empty());
  EXPECT_EQ(geog.num_shapes(), 0);
  EXPECT_EQ(geog.dimension(), -1);
  ASSERT_EQ(geog.Covering().size(), 0);

  auto point = MakeGeography("POINT (0 0)");
  EXPECT_FALSE(
      S2BooleanOperation::Intersects(geog.ShapeIndex(), point.ShapeIndex()));

  auto region = geog.Region();
  ASSERT_NE(region, nullptr);
  EXPECT_FALSE(region->Contains(S2LatLng::FromDegrees(0, 0).ToPoint()));
}

TEST_F(GeoArrowGeographyTest, InitGeometryZeroNodes) {
  GeoArrowGeography geog;
  geog.Init({nullptr, 0});
  EXPECT_TRUE(geog.is_empty());
  EXPECT_EQ(geog.num_shapes(), 0);
  EXPECT_EQ(geog.dimension(), -1);
  ASSERT_EQ(geog.Covering().size(), 0);

  auto point = MakeGeography("POINT (0 0)");
  EXPECT_FALSE(
      S2BooleanOperation::Intersects(geog.ShapeIndex(), point.ShapeIndex()));

  auto region = geog.Region();
  ASSERT_NE(region, nullptr);
  EXPECT_FALSE(region->Contains(S2LatLng::FromDegrees(0, 0).ToPoint()));
}

TEST_F(GeoArrowGeographyTest, Point) {
  auto geog = MakeGeography("POINT (1 2)");
  EXPECT_EQ(geog.dimension(), 0);
  EXPECT_EQ(geog.num_shapes(), 1);
  // Check twice because the value is cached
  ASSERT_EQ(geog.Covering().size(), 1);
  ASSERT_EQ(geog.Covering().size(), 1);

  auto shape = geog.Shape(0);
  ASSERT_NE(shape, nullptr);
  EXPECT_EQ(shape->dimension(), 0);
  EXPECT_EQ(shape->num_edges(), 1);

  EXPECT_GE(geog.ShapeIndex().num_shape_ids(), 1);
}

TEST_F(GeoArrowGeographyTest, MultiPoint) {
  auto geog = MakeGeography("MULTIPOINT ((0 0), (1 1), (2 2))");
  EXPECT_EQ(geog.dimension(), 0);
  EXPECT_EQ(geog.num_shapes(), 1);
  // Check twice because the value is cached
  ASSERT_EQ(geog.Covering().size(), 3);
  ASSERT_EQ(geog.Covering().size(), 3);

  auto shape = geog.Shape(0);
  ASSERT_NE(shape, nullptr);
  EXPECT_EQ(shape->num_edges(), 3);
}

TEST_F(GeoArrowGeographyTest, EmptyPoint) {
  auto geog = MakeGeography("POINT EMPTY");
  EXPECT_EQ(geog.dimension(), 0);
  EXPECT_EQ(geog.num_shapes(), 1);
  ASSERT_EQ(geog.Covering().size(), 0);

  auto shape = geog.Shape(0);
  ASSERT_NE(shape, nullptr);
  EXPECT_EQ(shape->num_edges(), 0);
}

TEST_F(GeoArrowGeographyTest, Linestring) {
  auto geog = MakeGeography("LINESTRING (0 0, 1 1, 2 2)");
  EXPECT_EQ(geog.dimension(), 1);
  EXPECT_EQ(geog.num_shapes(), 1);
  ASSERT_GT(geog.Covering().size(), 0);

  auto shape = geog.Shape(0);
  ASSERT_NE(shape, nullptr);
  EXPECT_EQ(shape->dimension(), 1);
  EXPECT_EQ(shape->num_edges(), 2);
}

TEST_F(GeoArrowGeographyTest, MultiLinestring) {
  auto geog = MakeGeography("MULTILINESTRING ((0 0, 1 1), (2 2, 3 3, 4 4))");
  EXPECT_EQ(geog.dimension(), 1);
  EXPECT_EQ(geog.num_shapes(), 1);
  ASSERT_GT(geog.Covering().size(), 0);

  auto shape = geog.Shape(0);
  ASSERT_NE(shape, nullptr);
  EXPECT_EQ(shape->num_edges(), 3);  // 1 + 2
}

TEST_F(GeoArrowGeographyTest, Polygon) {
  auto geog = MakeGeography("POLYGON ((0 0, 1 0, 0 1, 0 0))");
  EXPECT_EQ(geog.dimension(), 2);
  EXPECT_EQ(geog.num_shapes(), 1);
  ASSERT_GT(geog.Covering().size(), 0);

  auto shape = geog.Shape(0);
  ASSERT_NE(shape, nullptr);
  EXPECT_EQ(shape->dimension(), 2);
  EXPECT_EQ(shape->num_edges(), 3);
}

TEST_F(GeoArrowGeographyTest, MultiPolygon) {
  auto geog = MakeGeography(
      "MULTIPOLYGON (((0 0, 1 0, 0 1, 0 0)), "
      "((10 10, 11 10, 10 11, 10 10)))");
  EXPECT_EQ(geog.dimension(), 2);
  EXPECT_EQ(geog.num_shapes(), 1);
  ASSERT_GT(geog.Covering().size(), 0);

  auto shape = geog.Shape(0);
  ASSERT_NE(shape, nullptr);
  EXPECT_EQ(shape->num_edges(), 6);  // 3 + 3
}

TEST_F(GeoArrowGeographyTest, GeometryCollectionThrows) {
  auto gc_geom = TestGeometry::FromWKT("GEOMETRYCOLLECTION (POINT (0 0))");
  GeoArrowGeography geog;
  // Not yet supported
  EXPECT_THROW(geog.Init(gc_geom.geom()), Exception);
}

TEST_F(GeoArrowGeographyTest, RegionNotNull) {
  auto geog = MakeGeography("POLYGON ((0 0, 1 0, 0 1, 0 0))");
  auto region = geog.Region();
  EXPECT_NE(region, nullptr);
}

TEST_F(GeoArrowGeographyTest, ShapeIndexIntersection) {
  auto points = MakeGeography("MULTIPOINT ((0 0), (1 1), (50 50))");

  // Polygon overlapping the first two points
  auto poly_near = MakeGeography("POLYGON ((-1 -1, 2 -1, 2 2, -1 2, -1 -1))");
  EXPECT_TRUE(S2BooleanOperation::Intersects(points.ShapeIndex(),
                                             poly_near.ShapeIndex()));

  // Polygon far from all points
  auto poly_far =
      MakeGeography("POLYGON ((80 80, 81 80, 81 81, 80 81, 80 80))");
  EXPECT_FALSE(S2BooleanOperation::Intersects(points.ShapeIndex(),
                                              poly_far.ShapeIndex()));
}

TEST_F(GeoArrowGeographyTest, Region) {
  auto geog = MakeGeography("POLYGON ((-1 -1, 2 -1, 2 2, -1 2, -1 -1))");
  auto region = geog.Region();
  EXPECT_TRUE(region->Contains(S2LatLng::FromDegrees(0, 0).ToPoint()));

  // Point regions are specialized to make them less expensive to create
  auto geog_point = MakeGeography("POINT (0 0)");
  auto point_region = geog_point.Region();
  EXPECT_TRUE(point_region->Contains(S2LatLng::FromDegrees(0, 0).ToPoint()));
}

TEST_F(GeoArrowGeographyTest, RegionReversedWinding) {
  auto geog = MakeGeography("POLYGON ((-1 -1, -1 2, 2 2, 2 -1, -1 -1))");
  auto region = geog.Region();
  EXPECT_TRUE(region->Contains(S2LatLng::FromDegrees(0, 0).ToPoint()));

  auto geog_oriented =
      MakeGeography("POLYGON ((-1 -1, -1 2, 2 2, 2 -1, -1 -1))", true);
  region = geog_oriented.Region();
  EXPECT_FALSE(region->Contains(S2LatLng::FromDegrees(0, 0).ToPoint()));
}

TEST_F(GeoArrowGeographyTest, MoveConstructor) {
  auto geog = MakeGeography("POLYGON ((-1 -1, 2 -1, 2 2, -1 2, -1 -1))");
  auto point = MakeGeography("POINT (0 0)");

  GeoArrowGeography moved(std::move(geog));
  EXPECT_TRUE(
      S2BooleanOperation::Intersects(moved.ShapeIndex(), point.ShapeIndex()));
}

TEST_F(GeoArrowGeographyTest, MoveAssignment) {
  auto geog = MakeGeography("POLYGON ((-1 -1, 2 -1, 2 2, -1 2, -1 -1))");
  auto point = MakeGeography("POINT (0 0)");

  GeoArrowGeography assigned;
  assigned = std::move(geog);
  EXPECT_TRUE(S2BooleanOperation::Intersects(assigned.ShapeIndex(),
                                             point.ShapeIndex()));
}
