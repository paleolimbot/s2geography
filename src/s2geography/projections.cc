
#include <s2/r2.h>
#include <s2/s1chord_angle.h>
#include <s2/s2latlng.h>
#include <s2/s2point.h>
#include <s2/s2pointutil.h>
#include <s2/s2projections.h>

#include "s2geography/geography.h"

namespace s2geography {

std::shared_ptr<S2::Projection> lnglat() {
  std::shared_ptr<S2::Projection> projection =
      std::make_shared<S2::PlateCarreeProjection>(180);
  return std::move(projection);
}

std::shared_ptr<S2::Projection> pseudo_mercator() {
  // the semi-major axis of the WGS 84 ellipsoid is 6378137 meters
  // -> half of the circumference of the sphere is PI * 6378137 =
  // 20037508.3427892
  std::shared_ptr<S2::Projection> projection =
      std::make_shared<S2::MercatorProjection>(20037508.3427892);
  return std::move(projection);
}

class OrthographicProjection : public S2::Projection {
 public:
  OrthographicProjection(const S2LatLng& centre) : centre_(centre) {
    z_axis_ = S2Point(0, 0, 1);
    y_axis_ = S2Point(0, 1, 0);
  }

  // Converts a point on the sphere to a projected 2D point.
  R2Point Project(const S2Point& p) const {
    S2Point out = S2::Rotate(p, z_axis_, -centre_.lng());
    out = S2::Rotate(out, y_axis_, centre_.lat());
    if (out.x() >= 0) {
      return R2Point(out.y(), out.z());
    } else {
      return R2Point(NAN, NAN);
    }
  }

  // Converts a projected 2D point to a point on the sphere.
  S2Point Unproject(const R2Point& p) const {
    if (std::isnan(p.x()) || std::isnan(p.y())) {
      throw Exception("Can't unproject orthographic for non-finite point");
    }

    double y = p.x();
    double z = p.y();
    double x = sqrt(1.0 - y * y - z * z);
    S2Point pp(x, y, z);
    S2Point out = S2::Rotate(pp, y_axis_, -centre_.lat());
    out = S2::Rotate(out, z_axis_, centre_.lng());
    return out;
  }

  R2Point FromLatLng(const S2LatLng& ll) const { return Project(ll.ToPoint()); }

  S2LatLng ToLatLng(const R2Point& p) const { return S2LatLng(Unproject(p)); }

  R2Point wrap_distance() const { return R2Point(0, 0); }

 private:
  S2LatLng centre_;
  S2Point z_axis_;
  S2Point y_axis_;
};

std::shared_ptr<S2::Projection> orthographic(const S2LatLng& centre) {
  std::shared_ptr<S2::Projection> projection =
      std::make_shared<OrthographicProjection>(centre);
  return std::move(projection);
}

}  // namespace s2geography
