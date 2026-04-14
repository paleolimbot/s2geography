
#pragma once

#include <s2/s2latlng_rect.h>
#include <s2/s2region_coverer.h>

#include "s2geography/geography.h"

namespace s2geography {

class GeoArrowGeography;

class LatLngRectBounder {
 public:
  void Clear();
  S2LatLngRect Finish() const;
  void Update(const GeoArrowGeography& value);

 private:
  S2LatLngRect BoundPoints(const GeoArrowGeography& value);
  S2LatLngRect BoundLines(const GeoArrowGeography& value);
  S2LatLngRect BoundLoops(const GeoArrowGeography& value);

  S2LatLngRect bounds_;
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

}  // namespace s2geography
