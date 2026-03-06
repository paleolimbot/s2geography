#pragma once

#include <s2/s2shape.h>

#include <vector>

#include "geoarrow/geoarrow.hpp"

namespace s2geography {

class GeoArrowLaxPolylineShape : public S2Shape {
 public:
  static constexpr TypeTag kTypeTag = 48493;

  GeoArrowLaxPolylineShape() = default;
  explicit GeoArrowLaxPolylineShape(struct GeoArrowGeometryView geom);

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
  int num_chains_;
  std::vector<int> num_vertices_;
  std::vector<int> num_edges_;
};

}  // namespace s2geography
