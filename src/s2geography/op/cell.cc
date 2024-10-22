
#include "s2geography/op/cell.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

#include "s2/s2cell_id.h"
#include "s2/s2latlng.h"

namespace s2geography::op::cell {

uint64_t FromToken::ExecuteScalar(const std::string_view cell_token) {
  return S2CellId::FromToken(cell_token.data()).id();
}

uint64_t FromLngLat::ExecuteScalar(std::pair<double, double> lnglat) {
  S2LatLng ll = S2LatLng::FromDegrees(lnglat.first, lnglat.second).Normalized();
  return S2CellId(ll.Normalized()).id();
}

LngLat ToLngLat::ExecuteScalar(const uint64_t cell_id) {
  S2CellId cell(cell_id);
  if (!cell.is_valid()) {
    return kInvalidLngLat;
  } else {
    S2LatLng ll = S2CellId(cell_id).ToLatLng();
    return {ll.lng().degrees(), ll.lat().degrees()};
  }
}

Point ToPoint::ExecuteScalar(const uint64_t cell_id) {
  S2CellId cell(cell_id);
  if (!cell.is_valid()) {
    return kInvalidPoint;
  } else {
    S2Point point = S2CellId(cell_id).ToPoint();
    return {point.x(), point.y(), point.z()};
  }
}

}  // namespace s2geography::op::cell
