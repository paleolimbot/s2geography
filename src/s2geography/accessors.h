
#pragma once

#include "s2geography/geography.h"
#include "s2geography/sedona_udf/sedona_extension.h"

namespace s2geography {

bool s2_is_collection(const Geography& geog);
int s2_dimension(const Geography& geog);
int s2_num_points(const Geography& geog);
bool s2_is_empty(const Geography& geog);
double s2_area(const Geography& geog);
double s2_length(const Geography& geog);
double s2_perimeter(const Geography& geog);
double s2_x(const Geography& geog);
double s2_y(const Geography& geog);
bool s2_find_validation_error(const Geography& geog, S2Error* error);

namespace arrow_udf {

void LengthKernel(struct SedonaCScalarKernel* out);
void AreaKernel(struct SedonaCScalarKernel* out);
void PerimeterKernel(struct SedonaCScalarKernel* out);
}  // namespace arrow_udf

}  // namespace s2geography
