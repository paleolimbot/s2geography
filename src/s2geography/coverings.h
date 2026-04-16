
#pragma once

#include <s2/s2latlng_rect.h>
#include <s2/s2region_coverer.h>

#include "s2geography/geoarrow-geography.h"
#include "s2geography/geography.h"
#include "s2geography/sedona_udf/sedona_extension.h"

namespace s2geography {

class LatLngRectBounder {
 public:
  void Clear();
  S2LatLngRect Finish() const;
  void Update(const GeoArrowGeography& value);
  bool is_empty() { return bounds_.is_empty(); }

 private:
  S2LatLngRect BoundPoints(const GeoArrowGeography& value);
  S2LatLngRect BoundLines(const GeoArrowGeography& value);
  S2LatLngRect BoundLoops(const GeoArrowGeography& value);

  S2LatLngRect bounds_{S2LatLngRect::Empty()};
  std::vector<S2Point> scratch_;
};

S2Point s2_point_on_surface(const Geography& geog, S2RegionCoverer& coverer);
void s2_covering(const Geography& geog, std::vector<S2CellId>* covering,
                 S2RegionCoverer& coverer);
void s2_interior_covering(const Geography& geog,
                          std::vector<S2CellId>* covering,
                          S2RegionCoverer& coverer);
void s2_covering_buffered(const ShapeIndexGeography& geog,
                          double distance_radians,
                          std::vector<S2CellId>* covering,
                          S2RegionCoverer& coverer);

namespace sedona_udf {

void CellIdFromPointKernel(struct SedonaCScalarKernel* out);
void CoveringCellIdsKernel(struct SedonaCScalarKernel* out);
void BoundingBoxKernel(struct SedonaCScalarKernel* out);

}  // namespace sedona_udf

}  // namespace s2geography
