
#pragma once

#include <s2/s2boolean_operation.h>

#include "s2geography/geography.h"
#include "s2geography/sedona_udf/sedona_extension.h"

namespace s2geography {

bool s2_intersects(const ShapeIndexGeography& geog1,
                   const ShapeIndexGeography& geog2,
                   const S2BooleanOperation::Options& options);

bool s2_equals(const ShapeIndexGeography& geog1,
               const ShapeIndexGeography& geog2,
               const S2BooleanOperation::Options& options);

bool s2_contains(const ShapeIndexGeography& geog1,
                 const ShapeIndexGeography& geog2,
                 const S2BooleanOperation::Options& options);

/// Pre-computes closed/open boundary options for efficient repeated
/// s2_touches() evaluation in a loop.
class TouchesPredicate {
 public:
  explicit TouchesPredicate(const S2BooleanOperation::Options& options);

  bool operator()(const ShapeIndexGeography& geog1,
                  const ShapeIndexGeography& geog2) const;

 private:
  S2BooleanOperation::Options closed_options_;
  S2BooleanOperation::Options open_options_;
};

bool s2_touches(const ShapeIndexGeography& geog1,
                const ShapeIndexGeography& geog2,
                const S2BooleanOperation::Options& options);

bool s2_intersects_box(const ShapeIndexGeography& geog1,
                       const S2LatLngRect& rect,
                       const S2BooleanOperation::Options& options,
                       double tolerance);

namespace sedona_udf {

void IntersectsKernel(struct SedonaCScalarKernel* out);
void ContainsKernel(struct SedonaCScalarKernel* out);
void EqualsKernel(struct SedonaCScalarKernel* out);

}  // namespace sedona_udf

}  // namespace s2geography
