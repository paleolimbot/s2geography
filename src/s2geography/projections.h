
#pragma once

#include "s2/s2projections.h"

namespace s2geography {

// Constructs the "plate carree" projection which maps coordinates on the sphere
// to (longitude, latitude) pairs.
// The x coordinates (longitude) span [-180, 180] and the y coordinates (latitude)
// span [-90, 90].
std::shared_ptr<S2::Projection> lnglat();

// Constructs the spherical Mercator projection. When used together with WGS84
// coordinates, known as the "Web Mercator" projection.
std::shared_ptr<S2::Projection> mercator();

// Constructs an orthographic projection with the given centre point. The
// resulting coordinates depict a single hemisphere of the globe as it appears
// from outer space, centred on the given point.
std::shared_ptr<S2::Projection> orthographic(const S2LatLng& centre);

}  // namespace s2geography
