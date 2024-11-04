
#pragma once

#include "s2/s2projections.h"

namespace s2geography {

// Constructs the "plate carree" projection which maps coordinates on the sphere
// to (longitude, latitude) pairs.
// The x coordinates (longitude) span [-180, 180] and the y coordinates (latitude)
// span [-90, 90].
S2::Projection* lnglat();

// Constructs the spherical Mercator projection. When used together with WGS84
// coordinates, known as the "Web Mercator" projection.
S2::Projection* mercator();

}  // namespace s2geography
