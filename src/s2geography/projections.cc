
#include "s2/s2projections.h"

#include <s2/r2.h>
#include <s2/s2latlng.h>
#include <s2/s2point.h>
#include <s2/s2pointutil.h>

namespace s2geography {

S2::Projection* lnglat() {
  static S2::PlateCarreeProjection projection(180);
  return &projection;
}

S2::Projection* mercator() {
  static S2::MercatorProjection projection(20037508.3427892);
  return &projection;
}

class OrthographicProjection: public S2::Projection {
public:
  OrthographicProjection(const S2LatLng& centre):
      centre_(centre) {
    z_axis_ = S2Point(0, 0, 1);
    y_axis_ = S2Point(0, 1, 0);
  }

  // Converts a point on the sphere to a projected 2D point.
  R2Point Project(const S2Point& p) const {
    S2Point out = S2::Rotate(p, z_axis_, -centre_.lng());
    out = S2::Rotate(out, y_axis_, centre_.lat());
    return R2Point(out.y(), out.z());
  }

  // Converts a projected 2D point to a point on the sphere.
  S2Point Unproject(const R2Point& p) const {
    double y = p.x();
    double z = p.y();
    double x = sqrt(1.0 - y * y - z * z);
    S2Point pp(x, y, z);
    S2Point out = S2::Rotate(pp, y_axis_, -centre_.lat());
    out = S2::Rotate(out, z_axis_, centre_.lng());
    return out;
  }

  R2Point FromLatLng(const S2LatLng& ll) const {
    return Project(ll.ToPoint());
  }

  S2LatLng ToLatLng(const R2Point& p) const {
    return S2LatLng(Unproject(p));
  }

  R2Point wrap_distance() const {return R2Point(0, 0); }

private:
  S2LatLng centre_;
  S2Point z_axis_;
  S2Point y_axis_;
};

S2::Projection* orthographic(const S2LatLng& centre) {
  static OrthographicProjection projection(centre);
  return &projection;
}

}  // namespace s2geography
