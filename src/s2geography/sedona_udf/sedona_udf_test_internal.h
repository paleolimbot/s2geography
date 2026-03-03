#pragma once

#include <gtest/gtest.h>

#include "geoarrow/geoarrow.hpp"
#include "nanoarrow/nanoarrow.hpp"
#include "s2geography.h"
#include "s2geography/s2geography_gtest_util.h"
#include "s2geography/sedona_udf/sedona_extension.h"

// We use a simple model for testing functions: types are either geoarrow.wkb
// or an Arrow type (the only ones used here are bool, int32, and double).
// We define some aliases here to ensure we can expand this if we need to and
// make it more clean in the tests what the input/output types actually are.
using ArrowTypeOrWKB = std::optional<enum ArrowType>;
#define ARROW_TYPE_WKB std::nullopt

// Create individual ArrowSchema values for each argument type
inline std::vector<nanoarrow::UniqueSchema> ArgSchemas(
    std::vector<ArrowTypeOrWKB> cols) {
  std::vector<nanoarrow::UniqueSchema> schemas;
  for (const auto& col : cols) {
    schemas.emplace_back();
    if (col) {
      NANOARROW_THROW_NOT_OK(
          ArrowSchemaInitFromType(schemas.back().get(), *col));
    } else {
      geoarrow::Wkb().InitSchema(schemas.back().get());
    }
  }
  return schemas;
}

// Create geoarrow.wkb argument from WKT
inline nanoarrow::UniqueArray ArgWkb(
    const std::vector<std::optional<std::string>>& values) {
  // Make a WKB array
  nanoarrow::UniqueArray array;
  NANOARROW_THROW_NOT_OK(
      ArrowArrayInitFromType(array.get(), NANOARROW_TYPE_STRING));
  NANOARROW_THROW_NOT_OK(ArrowArrayStartAppending(array.get()));
  for (const auto& value : values) {
    if (!value.has_value()) {
      NANOARROW_THROW_NOT_OK(ArrowArrayAppendNull(array.get(), 1));
    } else {
      ArrowStringView na_value{value->data(),
                               static_cast<int64_t>(value->size())};
      NANOARROW_THROW_NOT_OK(ArrowArrayAppendString(array.get(), na_value));
    }
  }

  NANOARROW_THROW_NOT_OK(ArrowArrayFinishBuildingDefault(array.get(), nullptr));

  // Cast it to WKB
  geoarrow::ArrayReader wkt_reader(GEOARROW_TYPE_WKT);
  wkt_reader.SetArray(array.get());
  geoarrow::ArrayWriter wkb_writer(GEOARROW_TYPE_WKB);
  NANOARROW_THROW_NOT_OK(
      wkt_reader.Visit(wkb_writer.visitor(), 0, values.size()));

  nanoarrow::UniqueArray out;
  wkb_writer.Finish(out.get());
  return out;
}

// Create an arrow array argument. Because we only expose functions whose
// arguments are geography, bool, int32, or double, we just use double here
// for simplicity.
inline nanoarrow::UniqueArray ArgArrow(
    enum ArrowType type, std::vector<std::optional<double>> values) {
  nanoarrow::UniqueArray array;
  NANOARROW_THROW_NOT_OK(ArrowArrayInitFromType(array.get(), type));
  NANOARROW_THROW_NOT_OK(ArrowArrayStartAppending(array.get()));

  for (const auto value : values) {
    if (value.has_value()) {
      switch (type) {
        case NANOARROW_TYPE_BOOL: {
          NANOARROW_THROW_NOT_OK(ArrowArrayAppendInt(array.get(), *value != 0));
          break;
        }
        case NANOARROW_TYPE_INT32: {
          NANOARROW_THROW_NOT_OK(
              ArrowArrayAppendInt(array.get(), static_cast<int32_t>(*value)));
          break;
        }
        case NANOARROW_TYPE_DOUBLE: {
          NANOARROW_THROW_NOT_OK(ArrowArrayAppendDouble(array.get(), *value));
          break;
        }
        default:
          throw std::runtime_error(
              "creating test data with type not supported");
      }
    } else {
      NANOARROW_THROW_NOT_OK(ArrowArrayAppendNull(array.get(), 1));
    }
  }

  NANOARROW_THROW_NOT_OK(ArrowArrayFinishBuildingDefault(array.get(), nullptr));
  return array;
}

// Test utility to create a SedonaCScalarKernelImpl from a kernel, call init,
// and check its output type.
inline void TestInitKernel(struct SedonaCScalarKernel* kernel,
                           struct SedonaCScalarKernelImpl* impl,
                           std::vector<ArrowTypeOrWKB> arg_types,
                           ArrowTypeOrWKB result_type) {
  kernel->new_impl(kernel, impl);

  auto schemas = ArgSchemas(arg_types);
  std::vector<const struct ArrowSchema*> schema_ptrs;
  for (auto& s : schemas) {
    schema_ptrs.push_back(s.get());
  }

  nanoarrow::UniqueSchema result_schema;
  ASSERT_EQ(
      impl->init(impl, schema_ptrs.data(), nullptr,
                 static_cast<int64_t>(schema_ptrs.size()), result_schema.get()),
      0)
      << impl->get_last_error(impl);

  if (result_type) {
    struct ArrowSchemaView out_type_view;
    ASSERT_EQ(ArrowSchemaViewInit(&out_type_view, result_schema.get(), nullptr),
              NANOARROW_OK);
    ASSERT_EQ(out_type_view.type, result_type);
  } else {
    auto type = ::geoarrow::GeometryDataType::Make(result_schema.get());
    ASSERT_EQ(type.id(), GEOARROW_TYPE_WKB);
  }
}

// Test utility to create argument arrays and call impl->execute() on an
// already-initialized SedonaCScalarKernelImpl.
// This exploits the property that all the functions we expose have geography
// arguments first.
inline void TestExecuteKernel(
    struct SedonaCScalarKernelImpl* impl, std::vector<ArrowTypeOrWKB> arg_types,
    ArrowTypeOrWKB result_type,
    std::vector<std::vector<std::optional<std::string>>> geography_args,
    std::vector<std::vector<std::optional<double>>> other_args,
    struct ArrowArray* out) {
  auto arg_type_it = arg_types.begin();
  std::vector<nanoarrow::UniqueArray> args;

  for (const auto& geometry_arg : geography_args) {
    ASSERT_NE(arg_type_it, arg_types.end());
    auto arg_type = *arg_type_it++;
    ASSERT_FALSE(arg_type.has_value());

    args.push_back(ArgWkb(geometry_arg));
  }

  for (const auto& arg : other_args) {
    ASSERT_NE(arg_type_it, arg_types.end());
    auto arg_type = *arg_type_it++;
    ASSERT_TRUE(arg_type.has_value());

    args.push_back(ArgArrow(*arg_type, arg));
  }

  ASSERT_EQ(arg_type_it, arg_types.end());

  std::vector<struct ArrowArray*> arg_pointers;
  for (auto& arg : args) {
    arg_pointers.push_back(arg.get());
  }

  // Compute n_rows: the maximum length across all args (scalars have length 1)
  int64_t n_rows = 1;
  for (auto& arg : args) {
    if (arg->length > n_rows) {
      n_rows = arg->length;
    }
  }

  ASSERT_EQ(
      impl->execute(impl, arg_pointers.data(),
                    static_cast<int64_t>(arg_pointers.size()), n_rows, out),
      0)
      << impl->get_last_error(impl);
}

// Check a non-geography result. Expected is an optional double here because
// we only expose functions whose return types are bool, int, or double
// (and all can be coerced to double).
inline void TestResultArrow(struct ArrowArray* result,
                            enum ArrowType result_type,
                            std::vector<std::optional<double>> expected) {
  std::vector<std::optional<double>> actual;
  nanoarrow::UniqueArrayView array_view;
  ArrowArrayViewInitFromType(array_view.get(), result_type);
  ASSERT_EQ(ArrowArrayViewSetArray(array_view.get(), result, nullptr),
            NANOARROW_OK);

  for (int64_t i = 0; i < array_view->length; i++) {
    if (ArrowArrayViewIsNull(array_view.get(), i)) {
      actual.push_back(ARROW_TYPE_WKB);
    } else if (result_type == NANOARROW_TYPE_BOOL) {
      actual.push_back(
          static_cast<double>(ArrowArrayViewGetIntUnsafe(array_view.get(), i)));
    } else {
      actual.push_back(ArrowArrayViewGetDoubleUnsafe(array_view.get(), i));
    }
  }

  if (actual == expected) {
    return;
  }

  if (actual.size() != expected.size()) {
    ASSERT_EQ(actual, expected);
  }

  for (size_t i = 0; i < actual.size(); i++) {
    if (actual[i].has_value() && expected[i].has_value()) {
      ASSERT_DOUBLE_EQ(*actual[i], *expected[i]) << " Element " << i;
    } else if (!actual[i].has_value() && !expected[i].has_value()) {
      continue;
    } else {
      ASSERT_EQ(actual, expected);
    }
  }
}

// Check a geography result. This rounds the WKT output to 6 decimal places
// to avoid floating point differences between platforms.
inline void TestResultGeography(
    struct ArrowArray* result,
    std::vector<std::optional<std::string>> expected) {
  ASSERT_EQ(result->length, expected.size());

  s2geography::geoarrow::Reader reader;
  s2geography::geoarrow::ImportOptions options;
  options.set_check(false);
  reader.Init(s2geography::geoarrow::Reader::InputType::kWKB, options);
  std::vector<std::unique_ptr<s2geography::Geography>> geogs;
  reader.ReadGeography(result, 0, result->length, &geogs);

  for (int64_t i = 0; i < result->length; i++) {
    SCOPED_TRACE("expected[" + std::to_string(i) + "]");
    if (geogs[i].get() == nullptr) {
      ASSERT_FALSE(expected[i].has_value());
    } else {
      ASSERT_TRUE(expected[i].has_value());
      ASSERT_THAT(*geogs[i], s2geography::WktEquals6(*expected[i]));
    }
  }
}
