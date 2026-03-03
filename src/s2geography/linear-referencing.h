
#pragma once

#include "s2geography/geography.h"
#include "s2geography/sedona_udf/sedona_extension.h"

namespace s2geography {

double s2_project_normalized(const Geography& geog1, const Geography& geog2);
S2Point s2_interpolate_normalized(const Geography& geog, double distance_norm);

namespace sedona_udf {

void LineInterpolatePointKernel(struct SedonaCScalarKernel* out);
void LineLocatePointKernel(struct SedonaCScalarKernel* out);

}  // namespace sedona_udf

}  // namespace s2geography
