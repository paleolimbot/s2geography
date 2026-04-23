
#pragma once

#include <s2/s2boolean_operation.h>

#include "s2geography/geography.h"
#include "s2geography/operation.h"
#include "s2geography/sedona_udf/sedona_extension.h"

namespace s2geography {

bool s2_intersects(const ShapeIndexGeography& geog1,
                   const ShapeIndexGeography& geog2,
                   const S2BooleanOperation::Options& options);

bool s2_intersects(const S2ShapeIndex& geog1, const S2ShapeIndex& geog2,
                   const S2BooleanOperation::Options& options);

bool s2_equals(const ShapeIndexGeography& geog1,
               const ShapeIndexGeography& geog2,
               const S2BooleanOperation::Options& options);

bool s2_equals(const S2ShapeIndex& geog1, const S2ShapeIndex& geog2,
               const S2BooleanOperation::Options& options);

bool s2_contains(const ShapeIndexGeography& geog1,
                 const ShapeIndexGeography& geog2,
                 const S2BooleanOperation::Options& options);

bool s2_contains(const S2ShapeIndex& geog1, const S2ShapeIndex& geog2,
                 const S2BooleanOperation::Options& options);

bool s2_touches(const ShapeIndexGeography& geog1,
                const ShapeIndexGeography& geog2,
                const S2BooleanOperation::Options& options);

bool s2_intersects_box(const ShapeIndexGeography& geog1,
                       const S2LatLngRect& rect,
                       const S2BooleanOperation::Options& options,
                       double tolerance);

bool s2_intersects_box(const S2ShapeIndex& geog1, const S2LatLngRect& rect,
                       const S2BooleanOperation::Options& options,
                       double tolerance);

std::unique_ptr<Operation> Intersects();
std::unique_ptr<Operation> Disjoint();
std::unique_ptr<Operation> Contains();
std::unique_ptr<Operation> Within();
std::unique_ptr<Operation> Equals();

namespace sedona_udf {

void IntersectsKernel(struct SedonaCScalarKernel* out,
                      bool prepare_arg0_scalar = true,
                      bool prepare_arg1_scalar = true);
void DisjointKernel(struct SedonaCScalarKernel* out,
                    bool prepare_arg0_scalar = true,
                    bool prepare_arg1_scalar = true);
void ContainsKernel(struct SedonaCScalarKernel* out,
                    bool prepare_arg0_scalar = true,
                    bool prepare_arg1_scalar = true);
void WithinKernel(struct SedonaCScalarKernel* out,
                  bool prepare_arg0_scalar = true,
                  bool prepare_arg1_scalar = true);
void EqualsKernel(struct SedonaCScalarKernel* out,
                  bool prepare_arg0_scalar = true,
                  bool prepare_arg1_scalar = true);

}  // namespace sedona_udf

}  // namespace s2geography
