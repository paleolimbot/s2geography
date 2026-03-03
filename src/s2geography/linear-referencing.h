
#pragma once

#include "s2geography/arrow_udf/arrow_udf.h"
#include "s2geography/geography.h"
#include "s2geography/sedona_udf/sedona_extension.h"

namespace s2geography {

double s2_project_normalized(const Geography& geog1, const Geography& geog2);
S2Point s2_interpolate_normalized(const Geography& geog, double distance_norm);

namespace arrow_udf {

std::unique_ptr<ArrowUDF> LineInterpolatePoint();

std::unique_ptr<ArrowUDF> LineLocatePoint();

void LineInterpolatePointKernel(struct SedonaCScalarKernel* out);
void LineLocatePointKernel(struct SedonaCScalarKernel* out);

}  // namespace arrow_udf

}  // namespace s2geography
