#pragma once

#include "s2geography/sedona_udf/sedona_extension.h"

namespace s2geography {

namespace sedona_udf {

/// \brief Kernel to convert planar geometry to geography (spherical)
void TessellateToGeog(struct SedonaCScalarKernel* out);

/// \brief Kernel to convert geography (spherical) to planar geometry
void TessellateToGeom(struct SedonaCScalarKernel* out);

/// \brief Kernel to segmentize geography along spherical edges
void Segmentize(struct SedonaCScalarKernel* out);

}  // namespace sedona_udf

}  // namespace s2geography
