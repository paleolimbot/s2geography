
#include "s2geography/op/cell.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

#include "s2/s2cell_id.h"
#include "s2/s2latlng.h"

namespace s2geography::op::cell {

uint64_t FromToken::ExecuteScalar(const std::string_view cell_token) {
  // This constructor may not work for all s2 versions
  return S2CellId::FromToken({cell_token.data(), cell_token.size()}).id();
}

uint64_t FromDebugString::ExecuteScalar(const std::string_view debug_string) {
  // This constructor may not work for all s2 versions
  return S2CellId::FromDebugString({debug_string.data(), debug_string.size()})
      .id();
}

uint64_t FromLngLat::ExecuteScalar(LngLat lnglat) {
  S2LatLng ll = S2LatLng::FromDegrees(lnglat[0], lnglat[1]).Normalized();
  return S2CellId(ll.Normalized()).id();
}

uint64_t FromPoint::ExecuteScalar(Point point) {
  S2Point pt(point[0], point[1], point[2]);
  return S2CellId(pt).id();
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

std::string_view ToToken::ExecuteScalar(const uint64_t cell_id) {
  last_result_ = S2CellId(cell_id).ToToken();
  return last_result_;
}

std::string_view ToDebugString::ExecuteScalar(const uint64_t cell_id) {
  last_result_ = S2CellId(cell_id).ToString();
  return last_result_;
}

}  // namespace s2geography::op::cell
