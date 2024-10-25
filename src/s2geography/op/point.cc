
#include "s2geography/op/point.h"

#include "s2/s2latlng.h"
#include "s2/s2point.h"

namespace s2geography {

namespace op {

namespace point {

LngLat ToLngLat::ExecuteScalar(const Point point) {
  S2Point pt(point[0], point[1], point[2]);
  S2LatLng ll(pt);
  return {ll.lng().degrees(), ll.lat().degrees()};
}

Point ToPoint::ExecuteScalar(const LngLat lnglat) {
  S2LatLng ll = S2LatLng::FromDegrees(lnglat[1], lnglat[0]);
  S2Point pt(ll.ToPoint());
  return {pt.x(), pt.y(), pt.z()};
}

}  // namespace point

}  // namespace op

}  // namespace s2geography
