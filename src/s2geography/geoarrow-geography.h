#pragma once

#include <s2/mutable_s2shape_index.h>
#include <s2/s2shape.h>
#include <s2/s2shape_index.h>

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
/// These structures are intended to be reused: the `Init()` method can be
/// called again once any derivative data structure is no longer needed. Using
/// this pattern should effectively reuse internal scratch space allocated
/// during initialization.
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

  /// \brief Create an empty point shape representing zero points
  GeoArrowPointShape() = default;

  /// \brief Convenience constructor that initializes a point shape
  ///
  /// Throws if geom neither a POINT nor a MULTIPOINT, or is a MULTIPOINT
  /// that contains EMPTY children.
  explicit GeoArrowPointShape(struct GeoArrowGeometryView geom);

  /// \brief Reset internal state such that this shape represents zero edges
  void Clear();

  /// \brief (Re)Initialize an existing shape
  ///
  /// Throws if geom neither a POINT nor a MULTIPOINT, or is a MULTIPOINT
  /// that contains EMPTY children.
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
/// LINESTRING/MULTILINESTRING geometries).
class GeoArrowLaxPolylineShape : public S2Shape {
 public:
  static constexpr TypeTag kTypeTag = 48493;

  /// \brief Create an empty polyline shape containing zero polylines
  GeoArrowLaxPolylineShape() { num_edges_.push_back(0); }

  /// \brief Convenience constructor that initializes a linestring shape
  ///
  /// Throws if geom neither a LINESTRING nor a MULTILINESTRING.
  explicit GeoArrowLaxPolylineShape(struct GeoArrowGeometryView geom);

  /// \brief Reset internal state such that this shape represents zero edges
  void Clear();

  /// \brief (Re)initialize an existing linestring shape
  ///
  /// Throws if geom neither a LINESTRING nor a MULTILINESTRING.
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
///
/// Note that S2's internal representation of a polygon is substantially
/// different than the simple features idiom. Briefly, polygons are composed
/// of zero or more "loops" whose vertex order defines which points are
/// contained by the loop. The NormalizeOrientation() helper is intended to
/// bridge these two idioms.
class GeoArrowLaxPolygonShape : public S2Shape {
 public:
  static constexpr TypeTag kTypeTag = 48494;

  /// \brief Create an empty polygon shape containing zero loops
  GeoArrowLaxPolygonShape() { num_edges_.push_back(0); }

  /// \brief Convenience constructor that initializes a polygon shape
  ///
  /// Throws if geometry is not a POLYGON or MULTIPOLYGON.
  explicit GeoArrowLaxPolygonShape(struct GeoArrowGeometryView geom);

  /// \brief Reset internal state such that this shape represents zero edges
  void Clear();

  /// \brief (Re)initialize a polygon shape
  ///
  /// Throws if geometry is not a POLYGON or MULTIPOLYGON.
  void Init(struct GeoArrowGeometryView geom);

  /// \brief Update loop orientations to ensure that "shell"s are wound
  /// counterclockwise and "hole"s are wound clockwise.
  ///
  /// The "shell" or "hole" of each loop is derived from its position
  /// within each input POLYGON, where the first child ring is considered a
  /// shell and subsequent child rings are considered holes.
  ///
  /// This does not modify the input geometry nodes nor does it force a copy
  /// of the data (but may force a copy of the entire GeoArrowGeometryNode
  /// array).
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

/// \brief Reusable Geography-like wrapper around a GeoArrowGeometryNode
///
/// This class is conceptually a union of zero or more of the
/// GeoArrowPointShape, GeoArrowLaxPolylineShape, and/or the
/// GeoArrowLaxPolygonShape. It is not currently a subclass of Geography because
/// it intentionally defers creating its index and provides a few other features
/// specifically targeted at rapid iteration over many GeoArrowGeometryViews
/// (this makes it difficult to comply with the const requirements of the
/// Geography which aren't necessary here).
class GeoArrowGeography {
 public:
  /// \brief Create an empty geography
  GeoArrowGeography() = default;

  GeoArrowGeography(const GeoArrowGeography&) = delete;
  GeoArrowGeography& operator=(const GeoArrowGeography&) = delete;
  GeoArrowGeography(GeoArrowGeography&& other);
  GeoArrowGeography& operator=(GeoArrowGeography&& other);

  /// \brief (Re)Initialize this Geography from a GeoArrowGeometryView
  ///
  /// If this view contains polygons, the internals will ensure that shells are
  /// treated as shells and holes are treated as holes regardless of winding
  /// direction.
  void Init(struct GeoArrowGeometryView geom);

  /// \brief (Re)Initialize this Geography from a GeoArrowGeometryView
  ///
  /// If this view contains polygons, the internals will assume that the winding
  /// order is the sole determinant of containment.
  void InitOriented(struct GeoArrowGeometryView geom);

  /// \brief A collection of cells that completely cover this geography
  ///
  /// This may be used with S2CellUnion utilities to check potential
  /// containment. This is lazily computed and may incur building an index. For
  /// Small point or multipoint geographies this will not incur the cost of
  /// building an index.
  const std::vector<S2CellId>& Covering();

  /// \brief Return a S2ShapeIndex representation of this geography
  ///
  /// This index is lazy: it will not be created until potentially this call,
  /// and will not be built until it is queried (see MutableS2ShapeIndex). Note
  /// that building an index of a small geography is comparatively very costly
  /// to brute force iteration if that is an option for a given algorithm.
  const S2ShapeIndex& ShapeIndex();

  /// \brief Return a Region representation of this geography
  ///
  /// This region is lazy: it will not be created until potentially this call.
  /// For (single) point geographies the implementation avoids creating or
  /// building an index.
  std::unique_ptr<S2Region> Region();

  /// \brief If this geography represents a single point, compute and return it
  ///
  /// This allows callers to use potentially much faster implementations of
  /// various algorithms that take an S2Point.
  std::optional<S2Point> Point() const;

  /// \brief Returns true if this geography has no edges
  bool is_empty() const;

  /// \brief Returns the dimension (0 for point, 1 for linestring, 2 for
  /// polygon), or -1 for geometry collections
  int dimension() const;

  /// \brief The number of shapes
  ///
  /// This is usually one (exactly one of the point, line, or polygon shape
  /// classes) but may be higher for geometrycollections.
  int num_shapes() const;

  /// \brief Retrieve a shape
  const S2Shape* Shape(int id) const;

 private:
  struct GeoArrowGeometryView geom_{};
  GeoArrowPointShape points_;
  GeoArrowLaxPolylineShape lines_;
  GeoArrowLaxPolygonShape polygons_;
  MutableS2ShapeIndex index_;
  std::vector<S2CellId> covering_;
  bool indexed_{};

  void InitIndex();
  void GetCellUnionBound(std::vector<S2CellId>* cell_ids);
};

/// @}

}  // namespace s2geography
