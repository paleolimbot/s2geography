
#pragma once

#include <absl/numeric/bits.h>
#include <s2/s2latlng.h>
#include <s2/s2shape.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "s2geography/arrow_abi.h"
#include "s2geography/macros.h"

namespace s2geography {

namespace internal {

/// \brief Internal flag we set to mark holes as we process polygon input
///
/// For the purposes of this flag, this is set purely by position (i.e.,
/// the first ring in each polygon is a shell and the rest are holes). This
/// is not validated.
static constexpr uint8_t kFlagS2GeographyIsHole =
    (static_cast<uint8_t>(1) << 7);

/// Convert a longitude and latitude to an S2Point
///
/// S2LatLng::ToPoint() has debug checks that lng and lat are within the
/// [-180, -90, 180, 90] range; however, we don't strictly require the
/// longitudes to be within this range and latitudes outside this range are
/// undefined behaviour.
///
/// A note that this function can be a bottleneck when an operation hasn't
/// effectively used an index or cached the resulting point sequence.
inline S2Point LngLatToPoint(double lng, double lat) {
  const S1Angle::SinCosPair phi = S1Angle::Degrees(lat).SinCos();
  const S1Angle::SinCosPair theta = S1Angle::Degrees(lng).SinCos();
  return S2Point(theta.cos * phi.cos, theta.sin * phi.cos, phi.sin);
}

/// \brief Visit "nodes" of a GeoArrowGeometryView
///
/// Briefly, a GeoArrowGeometryNode is either a sequence (if geometry_type
/// is GEOARROW_GEOMETRY_TYPE_POINT or GEOARROW_GEOMETRY_TYPE_LINESTRING)
/// whose size corresponds to the number of coordinates, or a container
/// (e.g., a polygon) whose size corresponds to the number of children
/// (e.g., the number of rings). The children always directly follow the
/// parent. Coordinates are views represented by a start const uint8_t*
/// and a stride for each dimension. This representation can express any
/// GeoArrow element including native and serialize representations. It
/// can also express most other geometry representations without copying
/// coordinates, at the expense of relying on unaligned access (memcpy)
/// and slightly inefficient representation of points. It optimizes for
/// forward iteration over an entire geometry, which is arguably the most
/// common access pattern. Among other things, it allows permuting axes
/// and reversing the order of a sequence without modifying the underlying
/// data.
template <typename Visit>
bool VisitGeoArrowNodes(struct GeoArrowGeometryView geom, Visit&& visit) {
  if (geom.size_nodes == 0) {
    return true;
  }

  const struct GeoArrowGeometryNode* end = geom.root + geom.size_nodes;
  for (const struct GeoArrowGeometryNode* node = geom.root; node < end;
       ++node) {
    if (!visit(node)) return false;
  }
  return true;
}

/// \brief Visit longitudes and latitudes of a single node
///
/// This is a building block for other visitors. All geometries are
/// interpreted by s2geography as longitude, latitude (in that order).
/// Visiting a single element is relatively cheap but not free: care
/// is taken in downstream internals to visit all members of a sequence
/// at once where possible.
template <typename Visit>
bool VisitLngLat(const struct GeoArrowGeometryNode* node, int64_t offset,
                 int64_t n, Visit&& visit) {
  S2GEOGRAPHY_DCHECK_GE(offset, 0);
  S2GEOGRAPHY_DCHECK_GE(n, 0);
  if (n == 0) {
    return true;
  }

  S2GEOGRAPHY_DCHECK_LE(offset + n, static_cast<int64_t>(node->size));

  const uint8_t* lngs = node->coords[0] + offset * node->coord_stride[0];
  const uint8_t* lats = node->coords[1] + offset * node->coord_stride[1];
  double lng, lat;

  if (node->flags & GEOARROW_GEOMETRY_NODE_FLAG_SWAP_ENDIAN) {
    uint64_t tmp;
    for (int64_t i = 0; i < n; ++i) {
      memcpy(&tmp, lngs, sizeof(double));
      tmp = absl::byteswap(tmp);
      memcpy(&lng, &tmp, sizeof(double));

      memcpy(&tmp, lats, sizeof(double));
      tmp = absl::byteswap(tmp);
      memcpy(&lat, &tmp, sizeof(double));
      if (!visit(lng, lat)) return false;

      lngs += node->coord_stride[0];
      lats += node->coord_stride[1];
    }
  } else {
    for (int64_t i = 0; i < n; ++i) {
      memcpy(&lng, lngs, sizeof(double));
      memcpy(&lat, lats, sizeof(double));
      if (!visit(lng, lat)) return false;

      lngs += node->coord_stride[0];
      lats += node->coord_stride[1];
    }
  }
  return true;
}

/// \brief A lossless vertex in lon/lat/z/m coordinates
///
/// Unlike an S2Point, this version of a vertex (1) does not incur rounding
/// errors from the roundtrip between S2LatLng and S2Point and (2) propagates
/// Z and M values.
struct GeoArrowVertex {
  /// \brief The longitude (X) value
  double lng{};
  /// \brief The latitude (Y) values
  double lat{};
  /// \brief The ZM portion of the coordinate
  ///
  /// Whether these values are missing, Z, M, or ZM depends on the
  /// dimensions of the sequence.
  double zm[2] = {0.0, 0.0};

  void SetPoint(const S2Point& pt) {
    S2LatLng ll(pt);
    lng = ll.lng().degrees();
    lat = ll.lat().degrees();
  }

  /// \brief Return the S2Point representation of this vertex
  S2Point ToPoint() const { return LngLatToPoint(lng, lat); }

  /// \brief Normalize the order of zm values such that this object
  /// always represents, x, y, z, and m (in that order)
  GeoArrowVertex Normalize(uint8_t dimensions) const {
    GeoArrowVertex v = *this;
    if (dimensions == GEOARROW_DIMENSIONS_XYM) {
      std::swap(v.zm[0], v.zm[1]);
    }
    return v;
  }

  friend bool operator==(const GeoArrowVertex& a, const GeoArrowVertex& b) {
    // Treat NaNs as equal so that missing ZM information does not affect
    // inequality (as long as it is consistently missing for both)
    auto eq = [](double x, double y) {
      return x == y || (std::isnan(x) && std::isnan(y));
    };
    return eq(a.lng, b.lng) && eq(a.lat, b.lat) && eq(a.zm[0], b.zm[0]) &&
           eq(a.zm[1], b.zm[1]);
  }

  friend bool operator!=(const GeoArrowVertex& a, const GeoArrowVertex& b) {
    return !(a == b);
  }
};

/// \brief A lossless edge in lon/lat/z/m coordinates
///
/// Similar to an S2Shape::Edge except propagates exact vertices and ZM
/// information.
struct GeoArrowEdge {
  /// \brief The first vertex of the edge
  GeoArrowVertex v0{};
  /// \brief The second vertex of the edge
  GeoArrowVertex v1{};

  /// \brief Interpolate a value along this edge
  ///
  /// - lng and lat values are interpolated along a spherical path
  /// - z and m values are interpolated linearly
  GeoArrowVertex Interpolate(double fraction);

  /// \brief Given an S2Point along this edge, interpolate
  ///
  /// - lng and lat values are derived directly from point unless
  ///   the point falls exactly on the start or end of the edge (
  ///   in which case the start or end vertex is returned directly
  ///   to minimize roundtrip rounding errors)
  /// - z and m values are interpolated linearly
  GeoArrowVertex Interpolate(const S2Point& point) const;

  /// \brief Normalize the order of zm values such that this object
  /// always represents, x, y, z, and m (in that order)
  GeoArrowEdge Normalize(uint8_t dimensions) {
    GeoArrowEdge e = *this;
    e.v0 = v0.Normalize(dimensions);
    e.v1 = v1.Normalize(dimensions);
    return e;
  }

  friend bool operator==(const GeoArrowEdge& a, const GeoArrowEdge& b) {
    return a.v0 == b.v0 && a.v1 == b.v1;
  }

  friend bool operator!=(const GeoArrowEdge& a, const GeoArrowEdge& b) {
    return !(a == b);
  }
};

/// \brief Visit native vertices
template <typename Visit>
bool VisitLngLatZM(const struct GeoArrowGeometryNode* node, int64_t offset,
                   int64_t n, Visit&& visit) {
  S2GEOGRAPHY_DCHECK_GE(offset, 0);
  S2GEOGRAPHY_DCHECK_GE(n, 0);
  if (n == 0) {
    return true;
  }

  S2GEOGRAPHY_DCHECK_LE(offset + n, static_cast<int64_t>(node->size));

  const uint8_t* lngs = node->coords[0] + offset * node->coord_stride[0];
  const uint8_t* lats = node->coords[1] + offset * node->coord_stride[1];
  const uint8_t* zm0s = node->coords[2] + offset * node->coord_stride[2];
  const uint8_t* zm1s = node->coords[3] + offset * node->coord_stride[3];
  struct GeoArrowVertex v;

  if (node->flags & GEOARROW_GEOMETRY_NODE_FLAG_SWAP_ENDIAN) {
    uint64_t tmp;
    for (int64_t i = 0; i < n; ++i) {
      memcpy(&tmp, lngs, sizeof(double));
      tmp = absl::byteswap(tmp);
      memcpy(&v.lng, &tmp, sizeof(double));

      memcpy(&tmp, lats, sizeof(double));
      tmp = absl::byteswap(tmp);
      memcpy(&v.lat, &tmp, sizeof(double));

      memcpy(&tmp, zm0s, sizeof(double));
      tmp = absl::byteswap(tmp);
      memcpy(&v.zm[0], &tmp, sizeof(double));

      memcpy(&tmp, zm1s, sizeof(double));
      tmp = absl::byteswap(tmp);
      memcpy(&v.zm[1], &tmp, sizeof(double));

      if (!visit(v)) return false;

      lngs += node->coord_stride[0];
      lats += node->coord_stride[1];
      zm0s += node->coord_stride[2];
      zm1s += node->coord_stride[3];
    }
  } else {
    for (int64_t i = 0; i < n; ++i) {
      memcpy(&v.lng, lngs, sizeof(double));
      memcpy(&v.lat, lats, sizeof(double));
      memcpy(&v.zm[0], zm0s, sizeof(double));
      memcpy(&v.zm[1], zm1s, sizeof(double));
      if (!visit(v)) return false;

      lngs += node->coord_stride[0];
      lats += node->coord_stride[1];
      zm0s += node->coord_stride[2];
      zm1s += node->coord_stride[3];
    }
  }
  return true;
}

/// \brief Visit a subset of vertices in a sequence as S2Points
template <typename Visit>
bool VisitVertices(const struct GeoArrowGeometryNode* node, int64_t offset,
                   int64_t n, Visit&& visit) {
  return VisitLngLat(node, offset, n, [&](double lng0, double lat0) {
    return visit(LngLatToPoint(lng0, lat0));
  });
}

/// \brief Visit a all vertices in a sequence as S2Points
template <typename Visit>
bool VisitVertices(const struct GeoArrowGeometryNode* node, Visit&& visit) {
  return VisitLngLat(node, 0, node->size, [&](double lng0, double lat0) {
    return visit(LngLatToPoint(lng0, lat0));
  });
}

/// \brief Visit a subset of edges in a sequence as S2Shape::Edges
template <typename Visit>
bool VisitEdges(const struct GeoArrowGeometryNode* node, int64_t offset,
                int64_t n, Visit&& visit) {
  S2GEOGRAPHY_DCHECK_GE(offset, 0);
  S2GEOGRAPHY_DCHECK_GE(n, 0);

  if (static_cast<int64_t>(node->size) < offset + n + 1) {
    return true;
  }

  S2Shape::Edge e;
  VisitVertices(node, offset, 1, [&](const S2Point& v) {
    e.v0 = v;
    return true;
  });

  return VisitVertices(node, offset + 1, n, [&](const S2Point& v) {
    e.v1 = v;
    if (!visit(e)) {
      return false;
    }

    e.v0 = e.v1;
    return true;
  });
}

/// \brief Visit all edges in a sequence as S2Shape::Edges
template <typename Visit>
bool VisitEdges(const struct GeoArrowGeometryNode* node, Visit&& visit) {
  if (node->size <= 1) {
    return true;
  }

  return VisitEdges(node, 0, node->size - 1, visit);
}

/// \brief Visit a subset of vertices in a sequence as GeoArrowVertex
template <typename Visit>
bool VisitNativeVertices(const struct GeoArrowGeometryNode* node,
                         int64_t offset, int64_t n, Visit&& visit) {
  return VisitLngLatZM(node, offset, n,
                       [&](const GeoArrowVertex& v) { return visit(v); });
}

/// \brief Visit all vertices in a sequence as GeoArrowVertex
template <typename Visit>
bool VisitNativeVertices(const struct GeoArrowGeometryNode* node,
                         Visit&& visit) {
  return VisitLngLatZM(node, 0, node->size,
                       [&](const GeoArrowVertex& v) { return visit(v); });
}

/// \brief Visit a subset of edges in a sequence as GeoArrowEdge
template <typename Visit>
bool VisitNativeEdges(const struct GeoArrowGeometryNode* node, int64_t offset,
                      int64_t n, Visit&& visit) {
  S2GEOGRAPHY_DCHECK_GE(offset, 0);
  S2GEOGRAPHY_DCHECK_GE(n, 0);

  if (static_cast<int64_t>(node->size) < offset + n + 1) {
    return true;
  }

  GeoArrowEdge e;
  VisitNativeVertices(node, offset, 1, [&](const GeoArrowVertex& v) {
    e.v0 = v;
    return true;
  });

  return VisitNativeVertices(node, offset + 1, n, [&](const GeoArrowVertex& v) {
    e.v1 = v;
    if (!visit(e)) {
      return false;
    }

    e.v0 = e.v1;
    return true;
  });
}

/// \brief Visit all edges in a sequence as GeoArrowEdge
template <typename Visit>
bool VisitNativeEdges(const struct GeoArrowGeometryNode* node, Visit&& visit) {
  if (node->size <= 1) {
    return true;
  }

  return VisitNativeEdges(node, 0, node->size - 1, visit);
}

}  // namespace internal

/// \brief A sequence of coordinates
///
/// This utility wrapper is a wrapper around a sequence of coordinates whose
/// view is defined by a GeoArrowGeometryNode. This wrapper facilitates visiting
/// raw storage for algorithms that require it. The raw storage always has
/// XY values corresponding to longitude and latitude. It is designed to be
/// trivial/cheap to create.
///
/// In general, copying vertices and edges out of a sequence has a cost,
/// although most of the things we use S2 to do have a much higher cost.
class GeoArrowChain {
 public:
  GeoArrowChain(const struct GeoArrowGeometryNode* node) : node(node) {}

  /// \brief The number of coordinates in the sequence
  uint32_t size() const { return node->size; }

  /// \brief The coordinate dimensions of this sequence
  uint8_t dimensions() const { return node->dimensions; }

  /// \brief Call a function for each S2Point in this sequence
  template <typename Visit>
  bool VisitVertices(Visit&& visit) const {
    return internal::VisitVertices(node, visit);
  }

  /// \brief Call a function for each S2Point in a slice of this sequence
  template <typename Visit>
  bool VisitVertices(int64_t offset, int64_t n, Visit&& visit) const {
    return internal::VisitVertices(node, offset, n, visit);
  }

  /// \brief Call a function for each pair of S2Points in this sequence
  template <typename Visit>
  bool VisitEdges(Visit&& visit) const {
    return internal::VisitEdges(node, visit);
  }

  /// \brief Call a function for each pair of S2Points in a slice of this
  /// sequence
  template <typename Visit>
  bool VisitEdges(int64_t offset, int64_t n, Visit&& visit) const {
    return internal::VisitEdges(node, offset, n, visit);
  }

  /// \brief Copy a single vertex out of this sequence
  S2Point vertex(int64_t i) const {
    S2Point v{};
    this->VisitVertices(i, 1, [&](const S2Point& pt) {
      v = pt;
      return true;
    });
    return v;
  }

  /// \brief Copy a single pair of vertices out of this sequence
  S2Shape::Edge edge(int64_t i) const {
    S2Shape::Edge e{};
    this->VisitEdges(i, 1, [&](const S2Shape::Edge& edge) {
      e = edge;
      return true;
    });
    return e;
  }

  /// \brief Call a function for each GeoArrowVertex in this sequence
  template <typename Visit>
  bool VisitNativeVertices(Visit&& visit) const {
    return internal::VisitNativeVertices(node, visit);
  }

  /// \brief Call a function for each GeoArrowVertex in a slice of this sequence
  template <typename Visit>
  bool VisitNativeVertices(int64_t offset, int64_t n, Visit&& visit) const {
    return internal::VisitNativeVertices(node, offset, n, visit);
  }

  /// \brief Call a function for each pair of GeoArrowVertex in this sequence
  template <typename Visit>
  bool VisitNativeEdges(Visit&& visit) const {
    return internal::VisitNativeEdges(node, visit);
  }

  /// \brief Call a function for each pair of GeoArrowVertex in a slice of this
  /// sequence
  template <typename Visit>
  bool VisitNativeEdges(int64_t offset, int64_t n, Visit&& visit) const {
    return internal::VisitNativeEdges(node, offset, n, visit);
  }

  /// \brief Copy a single native vertex out of this sequence
  internal::GeoArrowVertex native_vertex(int64_t i) const {
    internal::GeoArrowVertex v{};
    this->VisitNativeVertices(i, 1, [&](const internal::GeoArrowVertex& vtx) {
      v = vtx;
      return true;
    });
    return v;
  }

  /// \brief Copy a single pair of native vertices out of this sequence
  internal::GeoArrowEdge native_edge(int64_t i) const {
    internal::GeoArrowEdge e{};
    this->VisitNativeEdges(i, 1, [&](const internal::GeoArrowEdge& edge) {
      e = edge;
      return true;
    });
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
///
/// This class must be constructed with some scratch space and intentionally
/// does not define its own. This is because the S2Loop measures only operate on
/// spans of S2Point. This scratch space is lazily initialized and reused if
/// more than one method is called.
struct GeoArrowLoop : public GeoArrowChain {
 public:
  /// \brief Construct a loop and clear the scratch space
  ///
  /// Optionally tracks shell/hole for algorithms that require it; however,
  /// note that the loops provided by VisitLoops() are oriented correctly such
  /// that the curvature, signed area, centroid, and containment checks do not
  /// require checking the hole status.
  GeoArrowLoop(const struct GeoArrowGeometryNode* node,
               std::vector<S2Point>* scratch, bool is_hole = false)
      : GeoArrowChain(node), scratch_(scratch), is_hole_(is_hole) {
    scratch_->clear();
  }

  bool is_hole() const { return is_hole_; }

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

  /// \brief Check containment
  ///
  /// Checks containment based on a reference point (e.g., one obtained from an
  /// S2Shape). Note that winding order doesn't matter here and is not checked
  /// (the winding order is used by the S2Shape to calculate the reference
  /// point).
  bool BruteForceContains(const S2Point& pt,
                          const S2Shape::ReferencePoint& reference);

 protected:
  std::vector<S2Point>* scratch_{};
  bool is_hole_;

  void BuildScratch();
};

/// \brief Wrapper around a GeoArrowGeometryView
///
/// This is a utility wrapper around a sequence of nodes, used for
/// to keep the GeoArrow C structures and iteration out of the interface
/// except for this header.
class GeoArrowGeom {
 public:
  /// \brief Construct an empty sequence of nodes
  explicit GeoArrowGeom() = default;

  /// \brief Construct from a GeoArrowGeometryView
  GeoArrowGeom(struct GeoArrowGeometryView geom) : geom_(geom) {}

  /// \brief Construct from a node and size
  GeoArrowGeom(const struct GeoArrowGeometryNode* node, int64_t size)
      : geom_{node, size} {}

  /// \brief The root node
  const struct GeoArrowGeometryNode* root() const { return geom_.root; }

  /// \brief The number of nodes in this sequence
  int64_t size() const { return geom_.size_nodes; }

  template <typename Visit>
  bool VisitNodes(Visit&& visit) const {
    return internal::VisitGeoArrowNodes(
        geom_,
        [&](const struct GeoArrowGeometryNode* node) { return visit(node); });
  }

  /// \brief Visit sequences of coordinates
  ///
  /// Call a function of GeoArrowChain for each sequence (point or
  /// linestring) in this set of nodes. Other node types are ignored.
  template <typename Visit>
  bool VisitChains(Visit&& visit) const {
    return VisitNodes([&](const struct GeoArrowGeometryNode* node) {
      switch (node->geometry_type) {
        case GEOARROW_GEOMETRY_TYPE_POINT:
        case GEOARROW_GEOMETRY_TYPE_LINESTRING:
          return visit(GeoArrowChain(node));
        default:
          return true;
      }
    });
  }

  /// \brief Visit sequences of coordinates as GeoArrowLoop
  ///
  /// Call a function of GeoArrowLoop for each linestring sequence
  /// in this set of nodes. Other node types are ignored.
  /// This function does not check if the nodes are actually part of
  /// a polygon or not (e.g., so that the GeoArrowGeom may be a sequence
  /// of loops).
  template <typename Visit>
  bool VisitLoops(std::vector<S2Point>* scratch, Visit&& visit) {
    return VisitNodes([&](const struct GeoArrowGeometryNode* node) {
      switch (node->geometry_type) {
        case GEOARROW_GEOMETRY_TYPE_LINESTRING:
          return visit(GeoArrowLoop(
              node, scratch, node->flags & internal::kFlagS2GeographyIsHole));
        default:
          return true;
      }
    });
  }

  /// \brief Call a function for each vertex in this node set
  ///
  /// This visits all chains and all vertices, including the duplicate closed
  /// ring vertex in a polygon ring.
  template <typename Visit>
  bool VisitVertices(Visit&& visit) {
    return VisitChains(
        [&](GeoArrowChain chain) { return chain.VisitVertices(visit); });
  }

  /// \brief Call a function for each pair of vertices in this node set
  ///
  /// This visits all chains and all edges. Note that point geometries are not
  /// included in this visitation (i.e., only sequences with 2 or more
  /// coordinates are visited).
  template <typename Visit>
  bool VisitEdges(Visit&& visit) {
    return VisitChains(
        [&](GeoArrowChain chain) { return chain.VisitEdges(visit); });
  }

  /// \brief Call a function for each native vertex in this node set
  ///
  /// This visits all chains and all vertices, including the duplicate closed
  /// ring vertex in a polygon ring.
  template <typename Visit>
  bool VisitNativeVertices(Visit&& visit) {
    return VisitChains(
        [&](GeoArrowChain chain) { return chain.VisitNativeVertices(visit); });
  }

  /// \brief Call a function for each pair of native vertices in this node set
  ///
  /// This visits all chains and all edges. Note that point geometries are not
  /// included in this visitation (i.e., only sequences with 2 or more
  /// coordinates are visited).
  template <typename Visit>
  bool VisitNativeEdges(Visit&& visit) {
    return VisitChains(
        [&](GeoArrowChain chain) { return chain.VisitNativeEdges(visit); });
  }

 private:
  struct GeoArrowGeometryView geom_{};
};

}  // namespace s2geography
