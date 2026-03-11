
#include "s2geography/geoarrow-geography.h"

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

static constexpr uint8_t kFlagS2GeographyIsHole =
    (static_cast<uint8_t>(1) << 7);

void ReverseNodeInPlace(struct GeoArrowGeometryNode* node) {
  if (node->size <= 1) return;
  for (int i = 0; i < 4; ++i) {
    int64_t offset = (static_cast<int64_t>(node->size) - 1) *
                     static_cast<int64_t>(node->coord_stride[i]);
    node->coords[i] += offset;
    node->coord_stride[i] = -node->coord_stride[i];
  }
}

template <typename Visit>
void VisitNodes(struct GeoArrowGeometryView geom, Visit&& visit) {
  if (geom.size_nodes == 0) {
    return;
  }

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
          "Can't create GeoArrowPointShape() from geometry type " +
          std::string(GeometryTypeString(geom.root->geometry_type)));
  }

  if (geom_.size_nodes > std::numeric_limits<int>::max()) {
    throw Exception(
        "Can't create GeoArrowPointShape() from geometry with > INT_MAX "
        "points");
  }

  // This is rare but for now we check, as otherwise we might get an attempt to
  // visit the coordinate of a node that doesn't have any.
  VisitNodes(geom_, [&](const struct GeoArrowGeometryNode* node) {
    if (node->size == 0) {
      throw Exception(
          "Can't create GeoArrowPointShape() from MULTIPOINT with EMPTY "
          "components");
    }
  });
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

  if (geom_.size_nodes > std::numeric_limits<int>::max()) {
    throw Exception(
        "Can't create GeoArrowLaxPolylineShape() from geometry with > "
        "INT_MAX parts");
  }

  num_chains_ = geom_.size_nodes;
  num_edges_.resize(num_chains_ + 1);
  int64_t num_edges = 0;

  num_edges_[0] = 0;
  int64_t i = 1;
  VisitNodes(geom_, [&](const struct GeoArrowGeometryNode* node) {
    num_edges += node->size == 0 ? 0 : node->size - 1;
    if (num_edges > std::numeric_limits<int>::max()) {
      throw Exception(
          "Can't create GeoArrowLaxPolylineShape() from geometry with > "
          "INT_MAX edges");
    }

    num_edges_[i++] = num_edges;
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

  VisitNodes(geom, [&](const struct GeoArrowGeometryNode* node) {
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
          loops_.back().flags |= kFlagS2GeographyIsHole;
        }

        ++num_loops_;
        is_hole = true;
        break;
      default:
        throw Exception(
            "Can't create GeoArrowLaxPolygonShape() from geometry type" +
            std::string(GeometryTypeString(geom.root->geometry_type)));
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
  S2LatLng v[2];
  int vi = 0;
  VisitLngLat(&loops_[i], j, 2, [&](double lng, double lat) {
    v[vi++] = S2LatLng::FromDegrees(lat, lng);
  });
  return Edge(v[0].ToPoint(), v[1].ToPoint());
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

/// GeoArrowGeography

GeoArrowGeography::GeoArrowGeography(GeoArrowGeography&& other)
    : geom_(other.geom_),
      points_(std::move(other.points_)),
      lines_(std::move(other.lines_)),
      polygons_(std::move(other.polygons_)),
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
      if (points_.num_vertices() <= 100) {
        for (int i = 0; i < points_.num_edges(); ++i) {
          cell_ids->push_back(S2CellId(points_.vertex(i)));
        }
      }

      return;

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
    default:
      return 0;
  }
}

const S2Shape* GeoArrowGeography::Shape(int id) const {
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

    default:
      throw Exception("unsupported geometry type");
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

}  // namespace s2geography
