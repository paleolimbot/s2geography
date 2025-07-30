
#pragma once

#include <s2/s2convex_hull_query.h>

#include "s2geography/aggregator.h"
#include "s2geography/arrow_udf/arrow_udf.h"
#include "s2geography/geography.h"

namespace s2geography {

S2Point s2_centroid(const Geography& geog);
std::unique_ptr<Geography> s2_boundary(const Geography& geog);
std::unique_ptr<Geography> s2_convex_hull(const Geography& geog);

class CentroidAggregator : public Aggregator<S2Point> {
 public:
  void Add(const Geography& geog);
  void Merge(const CentroidAggregator& other);
  S2Point Finalize();

 private:
  S2Point centroid_;
};

class S2ConvexHullAggregator
    : public Aggregator<std::unique_ptr<PolygonGeography>> {
 public:
  void Add(const Geography& geog);
  std::unique_ptr<PolygonGeography> Finalize();

 private:
  S2ConvexHullQuery query_;
  std::vector<std::unique_ptr<Geography>> keep_alive_;
};

namespace arrow_udf {
/// \brief Instantiate an ArrowUDF for the s2_centroid() function
///
/// This ArrowUDF handles any GeoArrow array as input and produces
/// a geoarrow.wkb array as output.
std::unique_ptr<ArrowUDF> Centroid();
std::unique_ptr<ArrowUDF> ConvexHull();
std::unique_ptr<ArrowUDF> PointOnSurface();

}  // namespace arrow_udf

}  // namespace s2geography
