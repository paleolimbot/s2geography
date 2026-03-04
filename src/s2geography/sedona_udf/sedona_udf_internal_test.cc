#include <gtest/gtest.h>

#include "geoarrow/geoarrow.hpp"
#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/accessors-geog.h"
#include "s2geography/accessors.h"
#include "s2geography/linear-referencing.h"
#include "s2geography/sedona_udf/sedona_udf_test_internal.h"

// Tests the matching of the Arrow argument and also propagation of the CRS from
// the first argument in a binary kernel.
TEST(SedonaUdf, ArrowInputMatches) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::LineInterpolatePointKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  kernel.new_impl(&kernel, &impl);

  // Wrong number of arguments
  nanoarrow::UniqueSchema out;
  ASSERT_EQ(impl.init(&impl, nullptr, nullptr, 0, out.get()), NANOARROW_OK);
  ASSERT_EQ(out->release, nullptr);

  // Wrong argument type for the Arrow input (second arg)
  nanoarrow::UniqueSchema arg0;
  nanoarrow::UniqueSchema arg1;
  struct ArrowSchema* arg_ptrs[] = {arg0.get(), arg1.get()};

  // Correct first arg
  ::geoarrow::Wkb()
      .WithEdgeType(GEOARROW_EDGE_TYPE_SPHERICAL)
      .WithCrs("OGC:CRS84")
      .InitSchema(arg0.get());
  // Incorrect second arg
  ASSERT_EQ(ArrowSchemaInitFromType(arg1.get(), NANOARROW_TYPE_STRING),
            NANOARROW_OK);
  ASSERT_EQ(impl.init(&impl, arg_ptrs, nullptr, 2, out.get()), NANOARROW_OK);
  ASSERT_EQ(out->release, nullptr);

  // Correct second arg
  arg1.reset();
  ASSERT_EQ(ArrowSchemaInitFromType(arg1.get(), NANOARROW_TYPE_DOUBLE),
            NANOARROW_OK);
  ASSERT_EQ(impl.init(&impl, arg_ptrs, nullptr, 2, out.get()), NANOARROW_OK);
  ASSERT_NE(out->release, nullptr);
  auto out_type = ::geoarrow::GeometryDataType::Make(out.get());
  ASSERT_EQ(out_type.ToString(), "spherical geoarrow.wkb<OGC:CRS84>");

  impl.release(&impl);
  kernel.release(&kernel);
}

// Tests the matching of the Arrow argument and also propagation of the CRS from
// the first argument in a unary kernel.
TEST(SedonaUdf, GeographyInputMatches) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::CentroidKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  kernel.new_impl(&kernel, &impl);

  // Wrong number of arguments
  nanoarrow::UniqueSchema out;
  ASSERT_EQ(impl.init(&impl, nullptr, nullptr, 0, out.get()), NANOARROW_OK);
  ASSERT_EQ(out->release, nullptr);

  // Wrong argument type
  nanoarrow::UniqueSchema arg0;
  struct ArrowSchema* arg0_ptr = arg0.get();
  ASSERT_EQ(ArrowSchemaInitFromType(arg0.get(), NANOARROW_TYPE_INT32),
            NANOARROW_OK);
  ASSERT_EQ(impl.init(&impl, &arg0_ptr, nullptr, 1, out.get()), NANOARROW_OK);
  ASSERT_EQ(out->release, nullptr);

  // Wkb type with wrong edge type
  arg0.reset();
  ::geoarrow::Wkb().InitSchema(arg0.get());
  ASSERT_EQ(impl.init(&impl, &arg0_ptr, nullptr, 1, out.get()), NANOARROW_OK);
  ASSERT_EQ(out->release, nullptr);

  // Correct argument
  arg0.reset();
  ::geoarrow::Wkb()
      .WithEdgeType(GEOARROW_EDGE_TYPE_SPHERICAL)
      .WithCrs("OGC:CRS84")
      .InitSchema(arg0.get());
  ASSERT_EQ(impl.init(&impl, &arg0_ptr, nullptr, 1, out.get()), NANOARROW_OK);
  ASSERT_NE(out->release, nullptr);
  auto out_type = ::geoarrow::GeometryDataType::Make(out.get());
  ASSERT_EQ(out_type.ToString(), "spherical geoarrow.wkb<OGC:CRS84>");

  impl.release(&impl);
  kernel.release(&kernel);
}

// Check that the same SedonaCScalarKernelImpl with Arrow output can be executed
// more than once.
TEST(SedonaUdf, ArrowOutputMultipleExecuteCalls) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::AreaKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(
      TestInitKernel(&kernel, &impl, {ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE,
                        {{"POINT (0 1)", "LINESTRING (0 0, 0 1)",
                          "POLYGON ((0 0, 0 1, 1 0, 0 0))", std::nullopt}},
                        {}, out_array.get()));
  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array.get(), NANOARROW_TYPE_DOUBLE,
                      {0.0, 0.0, 6182489130.9071951, std::nullopt}));

  out_array.reset();
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE,
                        {{"POINT (1 2)", "LINESTRING (1 2, 3 4)",
                          "POLYGON ((0 0, 0 0.1, 0.1 0, 0 0))", std::nullopt}},
                        {}, out_array.get()));
  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array.get(), NANOARROW_TYPE_DOUBLE,
                      {0.0, 0.0, 61821784.015993997, std::nullopt}));

  impl.release(&impl);
  kernel.release(&kernel);
}

// Check that the same SedonaCScalarKernelImpl with Arrow output can be executed
// more than once.
TEST(SedonaUdf, GeographyOutputMultipleExecuteCalls) {
  struct SedonaCScalarKernel kernel;
  s2geography::sedona_udf::CentroidKernel(&kernel);
  struct SedonaCScalarKernelImpl impl;
  ASSERT_NO_FATAL_FAILURE(
      TestInitKernel(&kernel, &impl, {ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE,
                        {{"POINT (0 1)", "LINESTRING (0 0, 0 1)",
                          "POLYGON ((0 0, 0 1, 1 0, 0 0))", std::nullopt}},
                        {}, out_array.get()));
  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 1)", "POINT (0 0.5)",
                        "POINT (0.33335 0.333344)", std::nullopt}));

  out_array.reset();
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteKernel(&impl, {ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE,
                        {{"POINT (0 1)", "LINESTRING (0 0, 0 1)",
                          "POLYGON ((0 0, 0 0.1, 0.1 0, 0 0))", std::nullopt}},
                        {}, out_array.get()));
  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 1)", "POINT (0 0.5)",
                        "POINT (0.033333 0.033333)", std::nullopt}));

  impl.release(&impl);
  kernel.release(&kernel);
}
