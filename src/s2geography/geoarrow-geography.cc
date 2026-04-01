
#include "s2geography/geoarrow-geography.h"

#include <s2/s2edge_crosser.h>
#include <s2/s2edge_distances.h>
#include <s2/s2loop_measures.h>
#include <s2/s2point.h>
#include <s2/s2projections.h>
#include <s2/s2shape_index_region.h>
#include <s2/s2shapeutil_get_reference_point.h>

#include <algorithm>
#include <cstring>
#include <limits>

#include "s2/s2point_region.h"
#include "s2geography/geography_interface.h"

namespace s2geography {

namespace {

void ReverseNodeInPlace(struct GeoArrowGeometryNode* node) {
  if (node->size <= 1) return;
  for (int i = 0; i < 4; ++i) {
    int64_t offset = (static_cast<int64_t>(node->size) - 1) *
                     static_cast<int64_t>(node->coord_stride[i]);
    node->coords[i] += offset;
    node->coord_stride[i] = -node->coord_stride[i];
  }
}

bool AllLngLatNaN(struct GeoArrowGeometryView geom) {
  return internal::VisitGeoArrowNodes(
      geom, [&](const struct GeoArrowGeometryNode* node) {
        return internal::VisitLngLat(
            node, 0, node->size, [&](double lng, double lat) {
              return std::isnan(lng) && std::isnan(lat);
            });
      });
}

const char* GeometryTypeString(uint8_t geometry_type) {
  switch (geometry_type) {
    case GEOARROW_GEOMETRY_TYPE_POINT:
    case GEOARROW_GEOMETRY_TYPE_LINESTRING:
    case GEOARROW_GEOMETRY_TYPE_POLYGON:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
    case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
    case GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION:
      return GeoArrowGeometryTypeString(
          static_cast<enum GeoArrowGeometryType>(geometry_type));
    default:
      return "Unknown";
  }
}

}  // namespace

GeoArrowPointShape::GeoArrowPointShape(struct GeoArrowGeometryView geom) {
  Init(geom);
}

void GeoArrowPointShape::Clear() { geom_ = {nullptr, 0}; }

void GeoArrowPointShape::Init(struct GeoArrowGeometryView geom) {
  switch (geom.root->geometry_type) {
    case GEOARROW_GEOMETRY_TYPE_POINT:
      // Treat an empty point as MULTIPOINT EMPTY
      // geoarrow-c currently reads POINT EMPTY as nan nan instead of a
      // proper EMPTY
      if (geom.root->size == 0 || AllLngLatNaN(geom)) {
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
          "Can't create GeoArrowPointShape() from geometry type " +
          std::string(GeometryTypeString(geom.root->geometry_type)));
  }

  if (geom_.size() > std::numeric_limits<int>::max()) {
    throw Exception(
        "Can't create GeoArrowPointShape() from geometry with > INT_MAX "
        "points");
  }

  // This is rare but for now we check, as otherwise we might get an attempt to
  // visit the coordinate of a node that doesn't have any.
  geom_.VisitChains([&](GeoArrowChain chain) -> bool {
    if (chain.size() == 0) {
      throw Exception(
          "Can't create GeoArrowPointShape() from MULTIPOINT with EMPTY "
          "components");
    }
    return true;
  });
}

int GeoArrowPointShape::num_vertices() const {
  return static_cast<int>(geom_.size());
}

S2Point GeoArrowPointShape::vertex(int v) const {
  return GeoArrowChain(geom_.root() + v).vertex(0);
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

S2Shape::Chain GeoArrowPointShape::chain(int /*i*/) const {
  return Chain(0, num_vertices());
}

S2Shape::Edge GeoArrowPointShape::chain_edge(int /*i*/, int j) const {
  S2Point p = vertex(j);
  return Edge(p, p);
}

S2Shape::ChainPosition GeoArrowPointShape::chain_position(int e) const {
  return ChainPosition(0, e);
}

S2Shape::TypeTag GeoArrowPointShape::type_tag() const { return kTypeTag; }

internal::GeoArrowEdge GeoArrowPointShape::native_edge(int e) const {
  internal::GeoArrowVertex v = GeoArrowChain(geom_.root() + e).native_vertex(0);
  return internal::GeoArrowEdge{v, v};
}

internal::GeoArrowEdge GeoArrowPointShape::native_chain_edge(int /*i*/,
                                                             int j) const {
  internal::GeoArrowVertex v = GeoArrowChain(geom_.root() + j).native_vertex(0);
  return internal::GeoArrowEdge{v, v};
}

GeoArrowLaxPolylineShape::GeoArrowLaxPolylineShape(
    struct GeoArrowGeometryView geom) {
  Init(geom);
}

void GeoArrowLaxPolylineShape::Clear() {
  geom_ = {nullptr, 0};
  num_chains_ = 0;
  num_edges_.clear();
  num_edges_.push_back(0);
}

void GeoArrowLaxPolylineShape::Init(struct GeoArrowGeometryView geom) {
  if (geom.size_nodes == 0) {
    throw Exception(
        "Can't create GeoArrowLaxPolylineShape from input with zero nodes");
  }

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
          "Can't create GeoArrowLaxPolylineShape() from geometry type " +
          std::string(GeometryTypeString(geom.root->geometry_type)));
  }

  if (geom_.size() > std::numeric_limits<int>::max()) {
    throw Exception(
        "Can't create GeoArrowLaxPolylineShape() from geometry with > "
        "INT_MAX parts");
  }

  num_chains_ = geom_.size();
  num_edges_.resize(num_chains_ + 1);
  int64_t num_edges = 0;

  num_edges_[0] = 0;
  int64_t i = 1;
  geom_.VisitChains([&](GeoArrowChain chain) -> bool {
    num_edges += chain.size() == 0 ? 0 : chain.size() - 1;
    if (num_edges > std::numeric_limits<int>::max()) {
      throw Exception(
          "Can't create GeoArrowLaxPolylineShape() from geometry with > "
          "INT_MAX edges");
    }

    num_edges_[i++] = num_edges;
    return true;
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
  return GeoArrowChain(geom_.root() + i).edge(j);
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

internal::GeoArrowEdge GeoArrowLaxPolylineShape::native_edge(int e) const {
  ChainPosition pos = GeoArrowLaxPolylineShape::chain_position(e);
  return GeoArrowLaxPolylineShape::native_chain_edge(pos.chain_id, pos.offset);
}

internal::GeoArrowEdge GeoArrowLaxPolylineShape::native_chain_edge(
    int i, int j) const {
  return GeoArrowChain(geom_.root() + i).native_edge(j);
}

// --- GeoArrowLaxPolygonShape ---

GeoArrowLaxPolygonShape::GeoArrowLaxPolygonShape(
    struct GeoArrowGeometryView geom) {
  Init(geom);
}

void GeoArrowLaxPolygonShape::Clear() {
  geom_ = {nullptr, 0};
  num_loops_ = 0;
  num_edges_.clear();
  num_edges_.push_back(0);
  loops_.clear();
}

void GeoArrowLaxPolygonShape::Init(struct GeoArrowGeometryView geom) {
  if (geom.size_nodes == 0) {
    throw Exception(
        "Can't create GeoArrowLaxPolygonShape from input with zero nodes");
  }

  // Collect all ring (LINESTRING) nodes
  Clear();
  int64_t num_edges = 0;
  bool is_hole = false;

  internal::VisitGeoArrowNodes(
      geom, [&](const struct GeoArrowGeometryNode* node) {
        switch (node->geometry_type) {
          case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
            break;
          case GEOARROW_GEOMETRY_TYPE_POLYGON:
            is_hole = false;
            break;
          case GEOARROW_GEOMETRY_TYPE_LINESTRING:
            num_edges += node->size == 0 ? 0 : node->size - 1;
            if (num_edges > std::numeric_limits<int>::max()) {
              throw Exception(
                  "Can't create GeoArrowLaxPolygonShape from geometry with > "
                  "INT_MAX vertices");
            }

            num_edges_.push_back(static_cast<int>(num_edges));
            loops_.push_back(*node);

            if (is_hole) {
              loops_.back().flags |= internal::kFlagS2GeographyIsHole;
            }

            ++num_loops_;
            is_hole = true;
            break;
          default:
            throw Exception(
                "Can't create GeoArrowLaxPolygonShape() from geometry type " +
                std::string(GeometryTypeString(geom.root->geometry_type)));
        }
        return true;
      });

  geom_ = {loops_.data(), static_cast<int64_t>(loops_.size())};
}

void GeoArrowLaxPolygonShape::NormalizeOrientation() {
  for (auto& node : loops_) {
    GeoArrowLoop loop(&node, &point_scratch_);
    double curvature = loop.GetCurvature();
    bool is_hole = (node.flags & internal::kFlagS2GeographyIsHole) != 0;
    if (is_hole != (curvature < 0)) {
      ReverseNodeInPlace(&node);
    }
  }
}

int GeoArrowLaxPolygonShape::num_edges() const { return num_edges_.back(); }

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
  return Chain(num_edges_[i], num_edges_[i + 1] - num_edges_[i]);
}

S2Shape::Edge GeoArrowLaxPolygonShape::chain_edge(int i, int j) const {
  return GeoArrowChain(&loops_[i]).edge(j);
}

S2Shape::ChainPosition GeoArrowLaxPolygonShape::chain_position(int e) const {
  auto it = std::upper_bound(num_edges_.begin(), num_edges_.end(), e);
  if (it == num_edges_.begin() || it == num_edges_.end()) {
    throw Exception("Edge at position " + std::to_string(e) +
                    " does not exist");
  }

  int i = static_cast<int>(it - num_edges_.begin()) - 1;
  return ChainPosition(i, e - num_edges_[i]);
}

S2Shape::TypeTag GeoArrowLaxPolygonShape::type_tag() const { return kTypeTag; }

internal::GeoArrowEdge GeoArrowLaxPolygonShape::native_edge(int e) const {
  ChainPosition pos = GeoArrowLaxPolygonShape::chain_position(e);
  return GeoArrowLaxPolygonShape::native_chain_edge(pos.chain_id, pos.offset);
}

internal::GeoArrowEdge GeoArrowLaxPolygonShape::native_chain_edge(int i,
                                                                  int j) const {
  return GeoArrowChain(&loops_[i]).native_edge(j);
}

bool GeoArrowLaxPolygonShape::BruteForceContains(
    const S2Point& pt, const S2Shape::ReferencePoint& reference) const {
  bool inside = reference.contained;
  if (is_empty() || is_full()) {
    return inside;
  }

  geom_.VisitChains([&](GeoArrowChain loop) {
    if (loop.size() < 2) {
      return true;
    }

    S2Point v0 = loop.vertex(0);
    S2CopyingEdgeCrosser crosser(reference.point, pt, v0);
    loop.VisitVertices(1, loop.size() - 1, [&](const S2Point& vertex) {
      inside ^= crosser.EdgeOrVertexCrossing(vertex);
      return true;
    });
    return true;
  });

  return inside;
}

bool GeoArrowLaxPolygonShape::BruteForceContains(const S2Point& pt) const {
  return BruteForceContains(pt, GetReferencePoint());
}

/// GeoArrowGeography

GeoArrowGeography::GeoArrowGeography(GeoArrowGeography&& other)
    : geom_(other.geom_),
      points_(std::move(other.points_)),
      lines_(std::move(other.lines_)),
      polygons_(std::move(other.polygons_)),
      covering_(std::move(other.covering_)),
      index_() {
  // index_ needs to be rebuilt
  index_.Clear();
  indexed_ = false;
}

GeoArrowGeography& GeoArrowGeography::operator=(GeoArrowGeography&& other) {
  if (this != &other) {
    geom_ = other.geom_;
    points_ = std::move(other.points_);
    lines_ = std::move(other.lines_);
    polygons_ = std::move(other.polygons_);
    covering_ = std::move(other.covering_);
    // index_ needs to be rebuilt
    index_.Clear();
    indexed_ = false;
  }
  return *this;
}

void GeoArrowGeography::Init(struct GeoArrowGeometryView geom) {
  InitOriented(geom);
  polygons_.NormalizeOrientation();
}

void GeoArrowGeography::InitOriented(struct GeoArrowGeometryView geom) {
  points_.Clear();
  lines_.Clear();
  polygons_.Clear();
  index_.Clear();
  covering_.clear();
  indexed_ = false;
  geom_ = geom;

  if (geom.size_nodes == 0) {
    return;
  }

  switch (geom.root->geometry_type) {
    case GEOARROW_GEOMETRY_TYPE_POINT:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
      points_.Init(geom);
      break;
    case GEOARROW_GEOMETRY_TYPE_LINESTRING:
    case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
      lines_.Init(geom);
      break;
    case GEOARROW_GEOMETRY_TYPE_POLYGON:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
      polygons_.Init(geom);
      break;
    // GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION:
    // Can be supported by walking the list and separating geometry types
    // but not yet.
    default:
      throw Exception(
          "Can't create GeoArrowGeography from geometry type " +
          std::string(GeometryTypeString(geom.root->geometry_type)));
  }
}

void GeoArrowGeography::GetCellUnionBound(std::vector<S2CellId>* cell_ids) {
  if (geom_.size_nodes == 0 || is_empty()) {
    return;
  }

  switch (geom_.root->geometry_type) {
    case GEOARROW_GEOMETRY_TYPE_POINT:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
      if (points_.num_vertices() <= 32) {
        points_.geom().VisitVertices([&](S2Point v) {
          cell_ids->push_back(S2CellId(v));
          return true;
        });
        return;
      }
      break;
    default:
      break;
  }

  Region()->GetCellUnionBound(cell_ids);
}

const std::vector<S2CellId>& GeoArrowGeography::Covering() {
  if (covering_.empty()) {
    GetCellUnionBound(&covering_);
  }

  return covering_;
}

const S2ShapeIndex& GeoArrowGeography::ShapeIndex() {
  InitIndex();
  return index_;
}

bool GeoArrowGeography::is_empty() const {
  if (geom_.size_nodes == 0) {
    return true;
  }

  switch (geom_.root->geometry_type) {
    case GEOARROW_GEOMETRY_TYPE_POINT:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
      return points_.is_empty();
    case GEOARROW_GEOMETRY_TYPE_LINESTRING:
    case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
      return lines_.is_empty();
    case GEOARROW_GEOMETRY_TYPE_POLYGON:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
      return polygons_.is_empty();
    default:
      for (int i = 0; i < num_shapes(); ++i) {
        if (!Shape(i)->is_empty()) {
          return false;
        }
      }

      return true;
  }
}

std::optional<S2Point> GeoArrowGeography::Point() const {
  if (geom_.size_nodes == 0) {
    return std::nullopt;
  }

  switch (geom_.root->geometry_type) {
    case GEOARROW_GEOMETRY_TYPE_POINT:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
      if (points_.num_edges() == 1) {
        return points_.edge(0).v0;
      }
    default:
      return std::nullopt;
  }
}

int GeoArrowGeography::dimension() const {
  if (geom_.size_nodes == 0) {
    return -1;
  }

  switch (geom_.root->geometry_type) {
    case GEOARROW_GEOMETRY_TYPE_POINT:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
      return 0;
    case GEOARROW_GEOMETRY_TYPE_LINESTRING:
    case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
      return 1;
    case GEOARROW_GEOMETRY_TYPE_POLYGON:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
      return 2;
    default:
      return -1;
  }
}

int GeoArrowGeography::num_edges() const {
  return points_.num_edges() + lines_.num_edges() + polygons_.num_edges();
}

const GeoArrowPointShape* GeoArrowGeography::points() const { return &points_; }
const GeoArrowLaxPolylineShape* GeoArrowGeography::lines() const {
  return &lines_;
}
const GeoArrowLaxPolygonShape* GeoArrowGeography::polygons() const {
  return &polygons_;
}

int GeoArrowGeography::num_shapes() const {
  if (geom_.size_nodes == 0) {
    return 0;
  }

  switch (geom_.root->geometry_type) {
    case GEOARROW_GEOMETRY_TYPE_POINT:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
    case GEOARROW_GEOMETRY_TYPE_LINESTRING:
    case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
    case GEOARROW_GEOMETRY_TYPE_POLYGON:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
      return 1;
    case GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION:
      return 3;
    default:
      throw Exception("Unsupported geometry type");
  }
}

const S2Shape* GeoArrowGeography::Shape(int id) const {
  S2GEOGRAPHY_DCHECK_GE(geom_.size_nodes, 0);
  switch (geom_.root->geometry_type) {
    case GEOARROW_GEOMETRY_TYPE_POINT:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
      return &points_;

    case GEOARROW_GEOMETRY_TYPE_LINESTRING:
    case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
      return &lines_;

    case GEOARROW_GEOMETRY_TYPE_POLYGON:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
      return &polygons_;

    case GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION:
      switch (id) {
        case 0:
          return &points_;
        case 1:
          return &lines_;
        case 2:
          return &polygons_;
        default:
          throw Exception("GeometryCollection shape ids must be 0, 1, or 2");
      }
    default:
      throw Exception("Unsupported geometry type in Shape()");
  }
}

std::unique_ptr<S2Region> GeoArrowGeography::Region() {
  auto maybe_point = Point();
  if (maybe_point) {
    return std::make_unique<S2PointRegion>(*maybe_point);
  }

  InitIndex();
  return std::make_unique<S2ShapeIndexRegion<MutableS2ShapeIndex>>(&index_);
}

void GeoArrowGeography::InitIndex() {
  if (indexed_ || geom_.size_nodes == 0) {
    return;
  }

  switch (geom_.root->geometry_type) {
    case GEOARROW_GEOMETRY_TYPE_POINT:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
      index_.Add(std::make_unique<S2ShapeWrapper>(&points_));
      break;
    case GEOARROW_GEOMETRY_TYPE_LINESTRING:
    case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
      index_.Add(std::make_unique<S2ShapeWrapper>(&lines_));
      break;
    case GEOARROW_GEOMETRY_TYPE_POLYGON:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
      index_.Add(std::make_unique<S2ShapeWrapper>(&polygons_));
      break;
    // GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION:
    // Can be supported by walking the list and separating geometry types
    // but not yet.
    default:
      throw Exception(
          "Can't create index from geometry type " +
          std::string(GeometryTypeString(geom_.root->geometry_type)));
  }

  indexed_ = true;
}

std::pair<int, int> GeoArrowGeography::ResolveGlobalEdgeId(
    int global_edge_id) const {
  if (geom_.size_nodes == 0) {
    return std::make_pair(-1, -1);
  }

  switch (geom_.root->geometry_type) {
    case GEOARROW_GEOMETRY_TYPE_POINT:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
    case GEOARROW_GEOMETRY_TYPE_LINESTRING:
    case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
    case GEOARROW_GEOMETRY_TYPE_POLYGON:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
      return std::make_pair(0, global_edge_id);
    case GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION:
      break;
    default:
      throw Exception("unsupported geometry type");
  }

  if (global_edge_id < points_.num_edges()) {
    return std::make_pair(0, global_edge_id);
  }

  global_edge_id -= points_.num_edges();
  if (global_edge_id < lines_.num_edges()) {
    return std::make_pair(1, global_edge_id);
  }

  global_edge_id -= lines_.num_edges();
  return std::make_pair(2, global_edge_id);
}

internal::GeoArrowEdge GeoArrowGeography::native_edge(int shape_id,
                                                      int edge_id) const {
  S2GEOGRAPHY_DCHECK_GE(geom_.size_nodes, 0);
  switch (geom_.root->geometry_type) {
    case GEOARROW_GEOMETRY_TYPE_POINT:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
      return points_.native_edge(edge_id);
    case GEOARROW_GEOMETRY_TYPE_LINESTRING:
    case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
      return lines_.native_edge(edge_id);
    case GEOARROW_GEOMETRY_TYPE_POLYGON:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
      return polygons_.native_edge(edge_id);
    case GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION:
      switch (shape_id) {
        case 0:
          return points_.native_edge(edge_id);
        case 1:
          return lines_.native_edge(edge_id);
        case 2:
          return polygons_.native_edge(edge_id);
        default:
          throw Exception("shape index out of bounds");
      }
    default:
      throw Exception("unsupported geometry type");
  }
}

namespace internal {

GeoArrowVertex GeoArrowEdge::Interpolate(double fraction) {
  if (fraction <= 0) {
    return v0;
  } else if (fraction >= 1) {
    return v1;
  }

  double dlng = (v1.lng - v0.lng) * fraction;
  double dlat = (v1.lat - v0.lat) * fraction;
  double dzm0 = (v1.zm[0] - v0.zm[0]) * fraction;
  double dzm1 = (v1.zm[1] - v0.zm[1]) * fraction;
  return {v0.lng + dlng, v0.lat + dlat, {v0.zm[0] + dzm0, v0.zm[1] + dzm1}};
}

GeoArrowVertex GeoArrowEdge::Interpolate(const S2Point& point) {
  auto pt0 = S2LatLng::FromDegrees(v0.lat, v0.lng).ToPoint();
  auto pt1 = S2LatLng::FromDegrees(v1.lat, v1.lng).ToPoint();

  // If the start and end are the same in lon/lat space, return the first vertex
  if (pt0 == pt1) {
    return v0;
  }

  // Find the edge fraction. Use this to interpolate ZM (or to directly return
  // a source vertex if the fraction is 0 or 1).
  double fraction = S2::GetDistanceFraction(point, pt0, pt1);
  if (fraction == 0) {
    return v0;
  } else if (fraction == 1) {
    return v1;
  }

  // Otherwise, interpolate ZM values in linear space but use the original point
  // to compute the output longitude and latitude.
  double dzm0 = (v1.zm[0] - v0.zm[0]) * fraction;
  double dzm1 = (v1.zm[1] - v0.zm[1]) * fraction;

  S2LatLng ll(point);
  return {ll.lng().degrees(),
          ll.lat().degrees(),
          {v0.zm[0] + dzm0, v0.zm[1] + dzm1}};
}

}  // namespace internal

double GeoArrowLoop::GetSignedArea() {
  BuildScratch();
  return S2::GetSignedArea(S2PointLoopSpan(*scratch_));
}

S2Point GeoArrowLoop::GetCentroid() {
  BuildScratch();
  return S2::GetCentroid(S2PointLoopSpan(*scratch_));
}

double GeoArrowLoop::GetCurvature() {
  BuildScratch();
  return S2::GetCurvature(S2PointLoopSpan(*scratch_));
}

bool GeoArrowLoop::BruteForceContains(
    const S2Point& pt, const S2Shape::ReferencePoint& reference) {
  if (size() < 4) {
    return reference.contained;
  }

  S2Point v0 = vertex(0);
  S2CopyingEdgeCrosser crosser(reference.point, pt, v0);
  bool inside = reference.contained;
  this->VisitVertices(1, size() - 1, [&](const S2Point& vertex_pt) {
    inside ^= crosser.EdgeOrVertexCrossing(vertex_pt);
    return true;
  });

  return inside;
}

void GeoArrowLoop::BuildScratch() {
  if (node->size == 0) {
    return;
  }

  if (scratch_->empty()) {
    this->VisitVertices(0, node->size - 1, [&](const S2Point& pt) {
      scratch_->push_back(pt);
      return true;
    });
  }
}

}  // namespace s2geography
