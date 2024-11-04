
#include "s2/s2projections.h"

namespace s2geography {

S2::Projection* lnglat() {
  static S2::PlateCarreeProjection projection(180);
  return &projection;
}

S2::Projection* mercator() {
  static S2::MercatorProjection projection(20037508.3427892);
  return &projection;
}

}  // namespace s2geography
