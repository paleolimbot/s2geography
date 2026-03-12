
#pragma once

#include <s2/s2latlng.h>
#include <s2/s2shape.h>

#include "geoarrow/geoarrow.h"

namespace s2geography {

namespace internal {

template <typename Visit>
void VisitGeoArrowNodes(struct GeoArrowGeometryView geom, Visit&& visit) {
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

template <typename Visit>
void VisitVertices(const struct GeoArrowGeometryNode* node, Visit&& visit) {
  VisitLngLat(node, 0, node->size, [&](double lng0, double lat0) {
    visit(S2LatLng::FromDegrees(lat0, lng0).ToPoint());
  });
}

template <typename Visit>
void VisitEdges(const struct GeoArrowGeometryNode* node, Visit&& visit) {
  if (node->size < 2) {
    return;
  }

  S2Shape::Edge e;
  double prev_lng, prev_lat;
  VisitLngLat(node, 0, 1, [&](double lng, double lat) {
    prev_lng = lng;
    prev_lat = lat;
  });

  VisitLngLat(node, 1, node->size - 1, [&](double lng, double lat) {
    e.v0 = S2LatLng::FromDegrees(prev_lat, prev_lng).ToPoint();
    e.v1 = S2LatLng::FromDegrees(lat, lng).ToPoint();
    visit(e);
    prev_lng = lng;
    prev_lat = lat;
  });
}

}  // namespace internal

class GeoArrowChain {
 public:
  GeoArrowChain(const struct GeoArrowGeometryNode* node) : node(node) {}

  uint32_t size() const { return node->size; }

  template <typename Visit>
  void VisitVertices(Visit&& visit) {
    internal::VisitVertices(node, visit);
  }

  template <typename Visit>
  void VisitEdges(Visit&& visit) {
    internal::VisitEdges(node, visit);
  }

 protected:
  const struct GeoArrowGeometryNode* node{};
};

struct GeoArrowLoop : public GeoArrowChain {
 public:
  GeoArrowLoop(const struct GeoArrowGeometryNode* node,
               std::vector<S2Point>* scratch)
      : GeoArrowChain(node), scratch_(scratch) {
    scratch_->clear();
  }

  double GetCurvature();

  double GetSignedArea();

  S2Point GetCentroid();

  double GetCurvatureMaxError();

 protected:
  std::vector<S2Point>* scratch_{};

  void BuildScratch();
};

class GeoArrowGeom {
 public:
  explicit GeoArrowGeom() = default;
  GeoArrowGeom(struct GeoArrowGeometryView geom) : geom_(geom) {}
  GeoArrowGeom(const struct GeoArrowGeometryNode* node, int64_t size)
      : geom_{node, size} {}

  const struct GeoArrowGeometryNode* root() const { return geom_.root; }
  int64_t size() const { return geom_.size_nodes; }

  template <typename Visit>
  void VisitChains(Visit&& visit) {
    internal::VisitGeoArrowNodes(geom_,
                                 [&](const struct GeoArrowGeometryNode* node) {
                                   switch (node->geometry_type) {
                                     case GEOARROW_GEOMETRY_TYPE_POINT:
                                     case GEOARROW_GEOMETRY_TYPE_LINESTRING:
                                       visit(GeoArrowChain(node));
                                       break;
                                     default:
                                       break;
                                   }
                                 });
  }

  template <typename Visit>
  void VisitLoops(std::vector<S2Point>* scratch, Visit&& visit) {
    internal::VisitGeoArrowNodes(geom_,
                                 [&](const struct GeoArrowGeometryNode* node) {
                                   switch (node->geometry_type) {
                                     case GEOARROW_GEOMETRY_TYPE_LINESTRING:
                                       visit(GeoArrowLoop(node, scratch));
                                       break;
                                     default:
                                       break;
                                   }
                                 });
  }

  template <typename Visit>
  void VisitVertices(Visit&& visit) {
    VisitChains([&](GeoArrowChain chain) { chain.VisitVertices(visit); });
  }

  template <typename Visit>
  void VisitEdges(Visit&& visit) {
    VisitChains([&](GeoArrowChain chain) { chain.VisitEdges(visit); });
  }

 private:
  struct GeoArrowGeometryView geom_{};
};

}  // namespace s2geography
