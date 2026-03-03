
#pragma once

#include "s2geography/geography.h"
#include "s2geography/sedona_udf/sedona_extension.h"

namespace s2geography {

double s2_distance(const ShapeIndexGeography& geog1,
                   const ShapeIndexGeography& geog2);
double s2_max_distance(const ShapeIndexGeography& geog1,
                       const ShapeIndexGeography& geog2);
S2Point s2_closest_point(const ShapeIndexGeography& geog1,
                         const ShapeIndexGeography& geog2);
std::pair<S2Point, S2Point> s2_minimum_clearance_line_between(
    const ShapeIndexGeography& geog1, const ShapeIndexGeography& geog2);

namespace arrow_udf {

void DistanceKernel(struct SedonaCScalarKernel* out);
void MaxDistanceKernel(struct SedonaCScalarKernel* out);
void ShortestLineKernel(struct SedonaCScalarKernel* out);
void ClosestPointKernel(struct SedonaCScalarKernel* out);
}  // namespace arrow_udf

}  // namespace s2geography
