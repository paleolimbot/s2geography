
#pragma once

#include "s2geography/geography.h"
#include "s2geography/sedona_udf/sedona_extension.h"

namespace s2geography {

double s2_distance(const ShapeIndexGeography& geog1,
                   const ShapeIndexGeography& geog2);
double s2_distance(const S2ShapeIndex& geog1, const S2ShapeIndex& geog2);

double s2_max_distance(const ShapeIndexGeography& geog1,
                       const ShapeIndexGeography& geog2);
double s2_max_distance(const S2ShapeIndex& geog1, const S2ShapeIndex& geog2);

S2Point s2_closest_point(const ShapeIndexGeography& geog1,
                         const ShapeIndexGeography& geog2);
S2Point s2_closest_point(const S2ShapeIndex& geog1, const S2ShapeIndex& geog2);

std::pair<S2Point, S2Point> s2_minimum_clearance_line_between(
    const ShapeIndexGeography& geog1, const ShapeIndexGeography& geog2);
std::pair<S2Point, S2Point> s2_minimum_clearance_line_between(
    const S2ShapeIndex& geog1, const S2ShapeIndex& geog2);

namespace sedona_udf {

void DistanceKernel(struct SedonaCScalarKernel* out,
                    bool prepare_arg0_scalar = true,
                    bool prepare_arg1_scalar = true);
void MaxDistanceKernel(struct SedonaCScalarKernel* out);
void ShortestLineKernel(struct SedonaCScalarKernel* out);
void ClosestPointKernel(struct SedonaCScalarKernel* out);

}  // namespace sedona_udf

}  // namespace s2geography
