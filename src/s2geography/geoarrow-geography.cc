
#include "s2geography/geoarrow-geography.h"

#include <s2/s2point.h>
#include <s2/s2projections.h>
#include <s2/s2shapeutil_get_reference_point.h>

#include <algorithm>
#include <cstring>
#include <limits>

#include "s2geography/geography.h"

namespace s2geography {

namespace {

static constexpr uint8_t kFlagS2GeographyIsHole =
    (static_cast<uint8_t>(1) << 7);

void ReverseNodeInPlace(struct GeoArrowGeometryNode* node) {
  if (node->size <= 1) return;
  for (int i = 0; i < 4; ++i) {
    node->coords[i] += (node->size - 1) * node->coord_stride[i];
    node->coord_stride[i] = -node->coord_stride[i];
  }
}

template <typename Visit>
void VisitNodes(struct GeoArrowGeometryView geom, Visit&& visit) {
  const struct GeoArrowGeometryNode* end = geom.root + geom.size_nodes;
  for (const struct GeoArrowGeometryNode* node = geom.root; node < end;
       ++node) {
    visit(node);
  }
}

template <typename Visit>
void VisitLngLat(const struct GeoArrowGeometryNode* node, int64_t offset,
                 int64_t n, Visit&& visit) {
  const uint8_t* lngs = node->coords[0] + offset * node->coord_stride[0];
  const uint8_t* lats = node->coords[1] + offset * node->coord_stride[1];
  double lng, lat;

  if (node->flags & GEOARROW_GEOMETRY_NODE_FLAG_SWAP_ENDIAN) {
    uint64_t tmp;
    for (int64_t i = 0; i < n; ++i) {
      memcpy(&tmp, lngs, sizeof(double));
      tmp = GEOARROW_BSWAP64(tmp);
      memcpy(&lng, &tmp, sizeof(double));

      memcpy(&tmp, lats, sizeof(double));
      tmp = GEOARROW_BSWAP64(tmp);
      memcpy(&lat, &tmp, sizeof(double));
      visit(lng, lat);

      lngs += node->coord_stride[0];
      lats += node->coord_stride[1];
    }
  } else {
    for (int64_t i = 0; i < n; ++i) {
      memcpy(&lng, lngs, sizeof(double));
      memcpy(&lat, lats, sizeof(double));
      visit(lng, lat);

      lngs += node->coord_stride[0];
      lats += node->coord_stride[1];
    }
  }
}

}  // namespace

GeoArrowPointShape::GeoArrowPointShape(struct GeoArrowGeometryView geom) {
  Init(geom);
}

void GeoArrowPointShape::Init(struct GeoArrowGeometryView geom) {
  switch (geom.root->geometry_type) {
    case GEOARROW_GEOMETRY_TYPE_POINT:
      // Treat an empty point as MULTIPOINT EMPTY
      if (geom.root->size == 0) {
        geom_ = {nullptr, 0};
      } else {
        geom_ = geom;
      }
      break;
    case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
      geom_ = {geom.root + 1, geom.size_nodes - 1};
      break;
    default:
      throw Exception(
          "Can't create GeoArrowPointShape() from geometry of unknown type ");
  }

  if (geom_.size_nodes > std::numeric_limits<int>::max()) {
    throw Exception(
        "Can't create GeoArrowPointShape() from geometry with > INT_MAX "
        "points");
  }
}

int GeoArrowPointShape::num_vertices() const {
  return static_cast<int>(geom_.size_nodes);
}

S2Point GeoArrowPointShape::vertex(int v) const {
  S2LatLng ll;
  VisitLngLat(geom_.root + v, 0, 1, [&](double lng, double lat) {
    ll = S2LatLng::FromDegrees(lat, lng);
  });
  return ll.ToPoint();
}

int GeoArrowPointShape::num_edges() const { return num_vertices(); }

S2Shape::Edge GeoArrowPointShape::edge(int e) const {
  S2Point p = vertex(e);
  return Edge(p, p);
}

int GeoArrowPointShape::dimension() const { return 0; }

S2Shape::ReferencePoint GeoArrowPointShape::GetReferencePoint() const {
  return ReferencePoint::Contained(false);
}

int GeoArrowPointShape::num_chains() const {
  return num_vertices() == 0 ? 0 : 1;
}

S2Shape::Chain GeoArrowPointShape::chain(int i) const {
  return Chain(0, num_vertices());
}

S2Shape::Edge GeoArrowPointShape::chain_edge(int i, int j) const {
  S2Point p = vertex(j);
  return Edge(p, p);
}

S2Shape::ChainPosition GeoArrowPointShape::chain_position(int e) const {
  return ChainPosition(0, e);
}

S2Shape::TypeTag GeoArrowPointShape::type_tag() const { return kTypeTag; }

GeoArrowLaxPolylineShape::GeoArrowLaxPolylineShape(
    struct GeoArrowGeometryView geom) {
  Init(geom);
}

void GeoArrowLaxPolylineShape::Init(struct GeoArrowGeometryView geom) {
  switch (geom.root->geometry_type) {
    case GEOARROW_GEOMETRY_TYPE_LINESTRING:
      if (geom.root->size == 0) {
        geom_ = {nullptr, 0};
      } else {
        geom_ = geom;
      }
      break;
    case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
      geom_ = {geom.root + 1, geom.size_nodes - 1};
      break;
    default:
      throw Exception(
          "Can't create GeoArrowLaxPolylineShape() from geometry of unknown "
          "type ");
  }

  if (geom_.size_nodes > std::numeric_limits<int>::max()) {
    throw Exception(
        "Can't create GeoArrowLaxPolylineShape() from geometry with > "
        "INT_MAX parts");
  }

  num_chains_ = geom_.size_nodes;
  num_edges_.resize(num_chains_ + 1);
  int64_t num_vertices = 0;
  int64_t num_edges = 0;

  num_edges_[0] = 0;
  int64_t i = 1;
  VisitNodes(geom_, [&](const struct GeoArrowGeometryNode* node) {
    num_vertices += node->size;
    num_edges += node->size == 0 ? 0 : node->size - 1;
    if (num_edges > std::numeric_limits<int>::max()) {
      throw Exception(
          "Can't create GeoArrowLaxPolylineShape() from geometry with > "
          "INT_MAX edges");
    }

    num_edges_[i] = num_edges;
    ++i;
  });
}

int GeoArrowLaxPolylineShape::num_edges() const { return num_edges_.back(); }

S2Shape::Edge GeoArrowLaxPolylineShape::edge(int e) const {
  ChainPosition pos = GeoArrowLaxPolylineShape::chain_position(e);
  return GeoArrowLaxPolylineShape::chain_edge(pos.chain_id, pos.offset);
}

int GeoArrowLaxPolylineShape::dimension() const { return 1; }

S2Shape::ReferencePoint GeoArrowLaxPolylineShape::GetReferencePoint() const {
  return ReferencePoint::Contained(false);
}

int GeoArrowLaxPolylineShape::num_chains() const { return num_chains_; }

S2Shape::Chain GeoArrowLaxPolylineShape::chain(int i) const {
  return Chain(num_edges_[i], num_edges_[i + 1] - num_edges_[i]);
}

S2Shape::Edge GeoArrowLaxPolylineShape::chain_edge(int i, int j) const {
  S2LatLng v[2];
  int vi = 0;
  VisitLngLat(geom_.root + i, j, 2, [&](double lng, double lat) {
    v[vi++] = S2LatLng::FromDegrees(lat, lng);
  });

  return Edge(v[0].ToPoint(), v[1].ToPoint());
}

S2Shape::ChainPosition GeoArrowLaxPolylineShape::chain_position(int e) const {
  auto it = std::upper_bound(num_edges_.begin(), num_edges_.end(), e);
  if (it == num_edges_.begin() || it == num_edges_.end()) {
    throw Exception("Edge at position " + std::to_string(e) +
                    " does not exist");
  }

  int i = static_cast<int>(it - num_edges_.begin()) - 1;
  return ChainPosition(i, e - num_edges_[i]);
}

S2Shape::TypeTag GeoArrowLaxPolylineShape::type_tag() const { return kTypeTag; }

// --- GeoArrowLaxPolygonShape ---

GeoArrowLaxPolygonShape::GeoArrowLaxPolygonShape(
    struct GeoArrowGeometryView geom) {
  Init(geom);
}

void GeoArrowLaxPolygonShape::Init(struct GeoArrowGeometryView geom) {
  // Collect all ring (LINESTRING) nodes
  num_loops_ = 0;
  num_vertices_.clear();
  num_vertices_.push_back(0);
  loops_.clear();
  int64_t total_vertices = 0;
  bool is_hole = false;

  VisitNodes(geom, [&](const struct GeoArrowGeometryNode* node) {
    switch (node->geometry_type) {
      case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
        break;
      case GEOARROW_GEOMETRY_TYPE_POLYGON:
        is_hole = false;
        break;
      case GEOARROW_GEOMETRY_TYPE_LINESTRING:
        total_vertices += node->size;
        if (total_vertices > std::numeric_limits<int>::max()) {
          throw Exception(
              "Can't create GeoArrowLaxPolygonShape from geometry with > "
              "INT_MAX vertices");
        }
        num_vertices_.push_back(static_cast<int>(total_vertices));
        loops_.push_back(*node);

        if (is_hole) {
          loops_.back().flags |= kFlagS2GeographyIsHole;
        }

        ++num_loops_;
        is_hole = true;
        break;
      default:
        throw Exception(
            "Can't create GeoArrowLaxPolygonShape() from geometry of unknown "
            "type ");
    }
  });

  geom_ = {loops_.data(), static_cast<int64_t>(loops_.size())};
}

void GeoArrowLaxPolygonShape::NormalizeOrientation() {
  for (auto& node : loops_) {
    point_scratch_.clear();
    VisitLngLat(&node, 0, node.size, [&](double lng, double lat) {
      point_scratch_.push_back(S2LatLng::FromDegrees(lat, lng).ToPoint());
    });

    double signed_area = S2::GetSignedArea(S2PointLoopSpan(point_scratch_));
    bool is_hole = (node.flags & kFlagS2GeographyIsHole) != 0;
    if (is_hole != (signed_area < 0)) {
      ReverseNodeInPlace(&node);
    }
  }
}

int GeoArrowLaxPolygonShape::num_loops() const { return num_loops_; }

int GeoArrowLaxPolygonShape::num_loop_vertices(int i) const {
  return num_vertices_[i + 1] - num_vertices_[i];
}

S2Point GeoArrowLaxPolygonShape::loop_vertex(int i, int j) const {
  S2LatLng ll;
  VisitLngLat(&loops_[i], j, 1, [&](double lng, double lat) {
    ll = S2LatLng::FromDegrees(lat, lng);
  });
  return ll.ToPoint();
}

int GeoArrowLaxPolygonShape::num_edges() const { return num_vertices_.back(); }

S2Shape::Edge GeoArrowLaxPolygonShape::edge(int e) const {
  ChainPosition pos = GeoArrowLaxPolygonShape::chain_position(e);
  return GeoArrowLaxPolygonShape::chain_edge(pos.chain_id, pos.offset);
}

int GeoArrowLaxPolygonShape::dimension() const { return 2; }

S2Shape::ReferencePoint GeoArrowLaxPolygonShape::GetReferencePoint() const {
  return s2shapeutil::GetReferencePoint(*this);
}

int GeoArrowLaxPolygonShape::num_chains() const { return num_loops_; }

S2Shape::Chain GeoArrowLaxPolygonShape::chain(int i) const {
  return Chain(num_vertices_[i], num_loop_vertices(i));
}

S2Shape::Edge GeoArrowLaxPolygonShape::chain_edge(int i, int j) const {
  int n = num_loop_vertices(i);
  int k = (j + 1 == n) ? 0 : j + 1;
  return Edge(loop_vertex(i, j), loop_vertex(i, k));
}

S2Shape::ChainPosition GeoArrowLaxPolygonShape::chain_position(int e) const {
  auto it = std::upper_bound(num_vertices_.begin(), num_vertices_.end(), e);
  if (it == num_vertices_.begin() || it == num_vertices_.end()) {
    throw Exception("Edge at position " + std::to_string(e) +
                    " does not exist");
  }

  int i = static_cast<int>(it - num_vertices_.begin()) - 1;
  return ChainPosition(i, e - num_vertices_[i]);
}

S2Shape::TypeTag GeoArrowLaxPolygonShape::type_tag() const { return kTypeTag; }

}  // namespace s2geography
