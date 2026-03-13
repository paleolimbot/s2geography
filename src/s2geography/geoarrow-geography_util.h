
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
void VisitLngLatEdges(const struct GeoArrowGeometryNode* node, int64_t offset,
                      int64_t n, Visit&& visit) {
  const uint8_t* lngs = node->coords[0] + offset * node->coord_stride[0];
  const uint8_t* lats = node->coords[1] + offset * node->coord_stride[1];
  double lng0, lat0, lng1, lat1;

  if (node->flags & GEOARROW_GEOMETRY_NODE_FLAG_SWAP_ENDIAN) {
    uint64_t tmp;

    // Extract the first vertex
    memcpy(&tmp, lngs, sizeof(double));
    tmp = GEOARROW_BSWAP64(tmp);
    memcpy(&lng0, &tmp, sizeof(double));

    memcpy(&tmp, lats, sizeof(double));
    tmp = GEOARROW_BSWAP64(tmp);
    memcpy(&lat0, &tmp, sizeof(double));

    lngs += node->coord_stride[0];
    lats += node->coord_stride[1];

    for (int64_t i = 0; i < n; ++i) {
      // Extract the next vertex
      memcpy(&tmp, lngs, sizeof(double));
      tmp = GEOARROW_BSWAP64(tmp);
      memcpy(&lng1, &tmp, sizeof(double));

      memcpy(&tmp, lats, sizeof(double));
      tmp = GEOARROW_BSWAP64(tmp);
      memcpy(&lat1, &tmp, sizeof(double));

      // Visit
      visit(lng0, lat0, lng1, lat1);

      // Move this vertex to the previous vertex and advance
      lng0 = lng1;
      lat0 = lat1;
      lngs += node->coord_stride[0];
      lats += node->coord_stride[1];
    }
  } else {
    // Extract the first vertex
    memcpy(&lng0, lngs, sizeof(double));
    memcpy(&lat0, lats, sizeof(double));
    lngs += node->coord_stride[0];
    lats += node->coord_stride[1];

    for (int64_t i = 0; i < n; ++i) {
      // Extract the next vertex
      memcpy(&lng1, lngs, sizeof(double));
      memcpy(&lat1, lats, sizeof(double));

      // Visit
      visit(lng0, lat0, lng1, lat1);

      // Move this vertex to the previous vertex and advance
      lng0 = lng1;
      lat0 = lat1;
      lngs += node->coord_stride[0];
      lats += node->coord_stride[1];
    }
  }
}

template <typename Visit>
void VisitVertices(const struct GeoArrowGeometryNode* node, int64_t offset,
                   int64_t n, Visit&& visit) {
  VisitLngLat(node, offset, n, [&](double lng0, double lat0) {
    visit(S2LatLng::FromDegrees(lat0, lng0).ToPoint());
  });
}

template <typename Visit>
void VisitVertices(const struct GeoArrowGeometryNode* node, Visit&& visit) {
  VisitLngLat(node, 0, node->size, [&](double lng0, double lat0) {
    visit(S2LatLng::FromDegrees(lat0, lng0).ToPoint());
  });
}

template <typename Visit>
void VisitEdges(const struct GeoArrowGeometryNode* node, int64_t offset,
                int64_t n, Visit&& visit) {
  if (node->size < (offset + n - 1)) {
    return;
  }

  S2Shape::Edge e;
  VisitLngLatEdges(node, offset, n,
                   [&](double lng0, double lat0, double lng1, double lat1) {
                     e.v0 = S2LatLng::FromDegrees(lat0, lng0).ToPoint();
                     e.v1 = S2LatLng::FromDegrees(lat1, lng1).ToPoint();
                     visit(e);
                   });
}

template <typename Visit>
void VisitEdges(const struct GeoArrowGeometryNode* node, Visit&& visit) {
  VisitEdges(node, 0, node->size - 1, visit);
}

}  // namespace internal

/// \brief A sequence of coordinates
///
/// This utility wrapper is a wrapper around a sequence of coordinates whose
/// view is defined by a GeoArrowGeometryNode. This wrapper facilitates visiting
/// raw storage for algorithms that require it. The raw storage always has
/// XY values corresponding to longitude and latitude.
class GeoArrowChain {
 public:
  GeoArrowChain(const struct GeoArrowGeometryNode* node) : node(node) {}

  /// \brief The number of coordinates in the sequence
  uint32_t size() const { return node->size; }

  /// \brief Call a function for each S2Point in this sequence
  template <typename Visit>
  void VisitVertices(Visit&& visit) {
    internal::VisitVertices(node, visit);
  }

  /// \brief Call a function for each S2Point in a slice of this sequence
  template <typename Visit>
  void VisitVertices(int64_t offset, int64_t n, Visit&& visit) {
    internal::VisitVertices(node, offset, n, visit);
  }

  template <typename Visit>
  void VisitEdges(Visit&& visit) {
    internal::VisitEdges(node, visit);
  }

  template <typename Visit>
  void VisitEdges(int64_t offset, int64_t n, Visit&& visit) {
    internal::VisitEdges(node, offset, n, visit);
  }

  S2Point vertex(int64_t i) {
    S2Point v;
    this->VisitVertices(i, 1, [&](const S2Point& pt) { v = pt; });
    return v;
  }

  S2Shape::Edge edge(int64_t i) {
    S2Shape::Edge e;
    this->VisitEdges(i, 1, [&](const S2Shape::Edge& edge) { e = edge; });
    return e;
  }

 protected:
  const struct GeoArrowGeometryNode* node{};
};

/// \brief A sequence of coordinates forming a closed loop
///
/// This utility wrapper is a wrapper around a sequence of coordinates whose
/// view is defined by a GeoArrowGeometryNode where the sequence specifically
/// defines a closed loop. This wrapper is specifically designed to provide
/// access to the loop measures and brute force containment algorithms that
/// are used across multiple functions.
struct GeoArrowLoop : public GeoArrowChain {
 public:
  GeoArrowLoop(const struct GeoArrowGeometryNode* node,
               std::vector<S2Point>* scratch)
      : GeoArrowChain(node), scratch_(scratch) {
    scratch_->clear();
  }

  /// \brief Get the sum of the turning angles
  ///
  /// Notably, this value is positive for counterclockwise loops and
  /// negative for clockwise loops.
  double GetCurvature();

  /// \brief Get the signed area of this loop in steradians
  ///
  /// Notably, this value is positive for counterclockwise loops and
  /// negative for clockwise loops.
  double GetSignedArea();

  /// \brief Get the centroid of this loop
  ///
  /// To make this value easier to compose with other centroids, it is
  /// scaled to the signed area of this loop.
  S2Point GetCentroid();

  bool BruteForceContains(const S2Point& pt,
                          const S2Shape::ReferencePoint& reference);

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
