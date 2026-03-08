#pragma once

#include <s2/s2shape.h>

#include <vector>

#include "geoarrow/geoarrow.hpp"

namespace s2geography {

/// \defgroup geoarrow-shapes GeoArrow S2Shape implementations
/// \brief S2Shape implementations backed by GeoArrowGeometryView
///
/// These classes provide lightweight S2Shape wrappers around
/// GeoArrowGeometryView, allowing direct use of GeoArrow-encoded geometry
/// (e.g., WKB buffers) in S2 spatial indexes without copying vertex data
/// into S2-native types. Each shape is only valid for the lifetime of the
/// underlying data pointed to by the wrapped GeoArrowGeometryView.
///
/// @{

/// \brief Point S2Shape implementation backed by a GeoArrowGeometryView
///
/// This shape represents zero or more points and can be initialized
/// from a POINT or MULTPOINT. It is based on the S2PointVectorShape.
/// The shape is only valid for the lifetime of the data pointed to
/// by the wrapped GeoArrowGeometryView (e.g., the WKB buffer containing
/// points).
class GeoArrowPointShape : public S2Shape {
 public:
  static constexpr TypeTag kTypeTag = 48492;

  GeoArrowPointShape() = default;

  GeoArrowPointShape(struct GeoArrowGeometryView geom);

  void Init(struct GeoArrowGeometryView geom);

  int num_vertices() const;
  S2Point vertex(int v) const;

  int num_edges() const override;
  Edge edge(int e) const override;
  int dimension() const override;
  ReferencePoint GetReferencePoint() const override;
  int num_chains() const override;
  Chain chain(int i) const override;
  Edge chain_edge(int i, int j) const override;
  ChainPosition chain_position(int e) const override;
  TypeTag type_tag() const override;

 private:
  struct GeoArrowGeometryView geom_{};
};

/// \brief Linestring S2Shape implementation backed by a GeoArrowGeometryView
///
/// This shape represents zero or more linestrings and can be initialized
/// from a LINESTRING or MULTILINESTRING. It is based on the S2LaxPolylineShape.
/// The shape is only valid for the lifetime of the data pointed to
/// by the wrapped GeoArrowGeometryView (e.g., the WKB buffer containing
/// points).
class GeoArrowLaxPolylineShape : public S2Shape {
 public:
  static constexpr TypeTag kTypeTag = 48493;

  GeoArrowLaxPolylineShape() { num_edges_.push_back(0); }

  GeoArrowLaxPolylineShape(struct GeoArrowGeometryView geom);

  void Init(struct GeoArrowGeometryView geom);

  int num_edges() const override;
  Edge edge(int e) const override;
  int dimension() const override;
  ReferencePoint GetReferencePoint() const override;
  int num_chains() const override;
  Chain chain(int i) const override;
  Edge chain_edge(int i, int j) const override;
  ChainPosition chain_position(int e) const override;
  TypeTag type_tag() const override;

 private:
  struct GeoArrowGeometryView geom_{};
  int num_chains_{};
  std::vector<int> num_edges_;
};

/// \brief Polygon S2Shape implementation backed by a GeoArrowGeometryView
///
/// This shape represents zero or more polygons and can be initialized
/// from a POLYGON or MULTIPOLYGON. It is based on the S2LaxPolygonShape.
/// The shape is only valid for the lifetime of the data pointed to
/// by the wrapped GeoArrowGeometryView (e.g., the WKB buffer containing
/// polygons).
///
/// Like the S2LaxPolygonShape, this class depends on the rings being
/// oriented. Use NormalizeOrientation() after Init() for input where the
/// winding order might be invalid; this ensures that "shell"s are oriented
/// counterclockwise and "holes" are oriented clockwise. This check may be
/// expensive.
class GeoArrowLaxPolygonShape : public S2Shape {
 public:
  static constexpr TypeTag kTypeTag = 48494;

  GeoArrowLaxPolygonShape() { num_edges_.push_back(0); }
  explicit GeoArrowLaxPolygonShape(struct GeoArrowGeometryView geom);

  void Init(struct GeoArrowGeometryView geom);

  void NormalizeOrientation();

  int num_edges() const override;
  Edge edge(int e) const override;
  int dimension() const override;
  ReferencePoint GetReferencePoint() const override;
  int num_chains() const override;
  Chain chain(int i) const override;
  Edge chain_edge(int i, int j) const override;
  ChainPosition chain_position(int e) const override;
  TypeTag type_tag() const override;

 private:
  struct GeoArrowGeometryView geom_{};
  int num_loops_{};
  // Cumulative edge counts per loop: num_edges_[0] = 0,
  // num_edges_[i+1] = total edges in loops 0..i
  std::vector<int> num_edges_;
  // Owned loops for O(1) lookup
  std::vector<struct GeoArrowGeometryNode> loops_;
  std::vector<S2Point> point_scratch_;
};

/// @}

}  // namespace s2geography
