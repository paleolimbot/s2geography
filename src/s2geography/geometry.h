#pragma once

#include "s2geography/geography.h"
#include "s2geography/sedona_udf/sedona_extension.h"

namespace s2geography {

namespace sedona_udf {

/// \brief Kernel to convert planar geometry to geography (spherical)
void ToGeographyKernel(struct SedonaCScalarKernel* out);

/// \brief Kernel to convert geography (spherical) to planar geometry
void ToGeometryKernel(struct SedonaCScalarKernel* out);

}  // namespace sedona_udf

}  // namespace s2geography
