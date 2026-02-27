
#pragma once

#include <s2/s2boolean_operation.h>

#include "s2geography/arrow_udf/arrow_udf.h"
#include "s2geography/geography.h"

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

namespace arrow_udf {
/// \brief Instantiate an ArrowUDF for the s2_intersects() function
///
/// This ArrowUDF handles any GeoArrow array as input and produces a boolean
/// array as output.
std::unique_ptr<ArrowUDF> Intersects();
std::unique_ptr<ArrowUDF> Contains();
std::unique_ptr<ArrowUDF> Equals();

}  // namespace arrow_udf

}  // namespace s2geography
