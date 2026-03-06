
#include <s2/s2point.h>
#include <s2/s2projections.h>
#include <s2/s2shape.h>

#include <cstring>

#include "geoarrow/geoarrow.hpp"
#include "s2geography/geography.h"

namespace s2geography {

namespace {

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
  const uint8_t* lngs = node->coords[0];
  const uint8_t* lats = node->coords[1];
  double lng, lat;

  if (node->flags & GEOARROW_GEOMETRY_NODE_FLAG_SWAP_ENDIAN) {
    uint64_t tmp;
    for (int64_t i = offset; i < n; ++i) {
      memcpy(&tmp, lngs, sizeof(double));
      tmp = GEOARROW_BSWAP64(tmp);
      memcpy(&lng, &tmp, sizeof(double));

      memcpy(&tmp, lats, sizeof(double));
      tmp = GEOARROW_BSWAP64(tmp);
      memcpy(&lat, &tmp, sizeof(double));
      visit(lng, lat);

      lngs += node->coord_stride[0];
      lngs += node->coord_stride[1];
    }
  } else {
    for (int64_t i = offset; i < n; ++i) {
      memcpy(&lng, lngs, sizeof(double));
      memcpy(&lat, lats, sizeof(double));
      visit(lng, lat);

      lngs += node->coord_stride[0];
      lngs += node->coord_stride[1];
    }
  }
}

}  // namespace

class GeoArrowLaxPolylineShape : public S2Shape {
 public:
  static constexpr TypeTag kTypeTag = 48493;

  GeoArrowLaxPolylineShape() = default;

  explicit GeoArrowLaxPolylineShape(struct GeoArrowGeometryView geom) {
    Init(geom);
  }

  void Init(struct GeoArrowGeometryView geom) {
    switch (geom.root->geometry_type) {
      case GEOARROW_GEOMETRY_TYPE_LINESTRING:
        geom_ = geom;
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

    if (geom_.size_nodes == 0) {
    }

    num_chains_ = geom_.size_nodes;
    num_vertices_.resize(num_chains_ + 1);
    num_edges_.resize(num_chains_ + 1);
    int64_t num_vertices = 0;
    int64_t num_edges = 0;
    int64_t i = 0;

    num_vertices_[0] = 0;
    num_edges_[0] = 0;
    VisitNodes(geom_, [&](const struct GeoArrowGeometryNode* node) {
      num_vertices += node->size;
      num_edges += 0 ? node->size == 0 : node->size - 1;
      if (num_edges > std::numeric_limits<int>::max()) {
        throw Exception(
            "Can't create GeoArrowLaxPpolylineShape from geometry with > "
            "INT_MAX edges");
      }

      num_vertices_[i] = num_vertices;
      num_edges_[i] = num_edges;
      ++i;
    });
  }

  int num_vertices() const { return num_vertices_.back(); }

  S2Point vertex(int v) const {
    for (int i = 0; i < num_chains_; i++) {
      if (v < num_vertices_[i + 1]) {
        S2LatLng ll;
        VisitLngLat(geom_.root + i, v - num_vertices_[i], 2,
                    [&](double lng, double lat) {
                      ll = S2LatLng::FromDegrees(lat, lng);
                    });
        return ll.ToPoint();
      }
    }

    throw Exception("Vertex at position " + std::to_string(v) +
                    " does not exist");
  }

  int num_edges() const override { return num_edges_.back(); }

  Edge edge(int e) const override { return Edge(vertex(e), vertex(e + 1)); }

  int dimension() const override { return 1; }

  ReferencePoint GetReferencePoint() const override {
    return ReferencePoint::Contained(false);
  }

  int num_chains() const override { return num_chains_; }

  Chain chain(int i) const override { return Chain(0, num_edges()); }

  Edge chain_edge(int i, int j) const override {
    S2LatLng v[2];
    int vi = 0;
    VisitLngLat(geom_.root + i, j, 2, [&](double lng, double lat) {
      v[vi++] = S2LatLng::FromDegrees(lat, lng);
    });

    return Edge(v[0].ToPoint(), v[1].ToPoint());
  }

  ChainPosition chain_position(int e) const override {
    for (int i = 0; i < num_chains_; i++) {
      if (e < num_edges_[i + 1]) {
        return ChainPosition(i, e - num_edges_[i]);
      }
    }

    throw Exception("Edge at position " + std::to_string(e) +
                    " does not exist");
  }

  TypeTag type_tag() const override { return kTypeTag; }

 private:
  struct GeoArrowGeometryView geom_{};
  int num_chains_;
  std::vector<int> num_vertices_;
  std::vector<int> num_edges_;
};

}  // namespace s2geography
