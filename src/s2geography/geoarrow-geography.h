#pragma once

#include <s2/s2shape.h>

#include <vector>

#include "geoarrow/geoarrow.hpp"

namespace s2geography {

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

class GeoArrowLaxPolylineShape : public S2Shape {
 public:
  static constexpr TypeTag kTypeTag = 48493;

  GeoArrowLaxPolylineShape() {
    num_vertices_.push_back(0);
    num_edges_.push_back(0);
  }

  GeoArrowLaxPolylineShape(struct GeoArrowGeometryView geom);

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
  int num_chains_{};
  std::vector<int> num_vertices_;
  std::vector<int> num_edges_;
};

class GeoArrowLaxPolygonShape : public S2Shape {
 public:
  static constexpr TypeTag kTypeTag = 48494;

  GeoArrowLaxPolygonShape() { num_vertices_.push_back(0); }
  explicit GeoArrowLaxPolygonShape(struct GeoArrowGeometryView geom);

  void Init(struct GeoArrowGeometryView geom);

  void NormalizeOrientation();

  int num_loops() const;
  int num_loop_vertices(int i) const;
  S2Point loop_vertex(int i, int j) const;

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
  // Cumulative vertex counts: num_vertices_[0] = 0,
  // num_vertices_[i+1] = total vertices in loops 0..i
  std::vector<int> num_vertices_;
  // Owned loops for O(1) lookup
  std::vector<struct GeoArrowGeometryNode> loops_;
  std::vector<S2Point> point_scratch_;
};

}  // namespace s2geography
