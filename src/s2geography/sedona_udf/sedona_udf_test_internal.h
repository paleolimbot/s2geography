#pragma once

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "geoarrow/geoarrow.hpp"
#include "nanoarrow/nanoarrow.hpp"
#include "s2geography.h"
#include "s2geography/sedona_udf/sedona_extension.h"

/// \brief An owning wrapper around a GeoArrowGeometry with utilities to
/// construct from WKT or WKB
class TestGeometry {
 public:
  TestGeometry() : oriented_(false) {
    GEOARROW_THROW_NOT_OK(nullptr, GeoArrowGeometryInit(&geom_));
  }

  ~TestGeometry() { GeoArrowGeometryReset(&geom_); }

  TestGeometry(const TestGeometry&) = delete;
  TestGeometry& operator=(const TestGeometry&) = delete;

  TestGeometry(TestGeometry&& other) noexcept
      : geom_(other.geom_),
        label_(std::move(other.label_)),
        oriented_(other.oriented_),
        data_(std::move(other.data_)) {
    GeoArrowGeometryInit(&other.geom_);
  }

  TestGeometry& operator=(TestGeometry&& other) noexcept {
    if (this != &other) {
      GeoArrowGeometryReset(&geom_);
      geom_ = other.geom_;
      GeoArrowGeometryInit(&other.geom_);
      label_ = std::move(other.label_);
      oriented_ = other.oriented_;
      data_ = std::move(other.data_);
    }
    return *this;
  }

  std::string ToWKT(int precision = 16) const {
    struct GeoArrowWKTWriter writer;
    GEOARROW_THROW_NOT_OK(nullptr, GeoArrowWKTWriterInit(&writer));
    writer.precision = precision;

    struct GeoArrowVisitor v;
    GeoArrowVisitorInitVoid(&v);
    GeoArrowWKTWriterInitVisitor(&writer, &v);

    GEOARROW_THROW_NOT_OK(nullptr, GeoArrowGeometryViewVisit(geom(), &v));

    nanoarrow::UniqueArray out;
    GEOARROW_THROW_NOT_OK(nullptr, S2GeographyGeoArrowWKTWriterFinish(
                                       &writer, out.get(), nullptr));
    GeoArrowWKTWriterReset(&writer);

    auto* offsets = reinterpret_cast<const int32_t*>(out->buffers[1]);
    auto* data = reinterpret_cast<const char*>(out->buffers[2]);
    std::string string_out(data, offsets[1]);

    // Work around a bug in the WKT writer for empty points
    if (string_out == "POINT (nan nan)") {
      return "POINT EMPTY";
    } else if (string_out == "POINT Z (nan nan nan)") {
      return "POINT Z EMPTY";
    } else if (string_out == "POINT M (nan nan nan)") {
      return "POINT M EMPTY";
    } else if (string_out == "POINT ZM (nan nan nan nan)") {
      return "POINT ZM EMPTY";
    }

    return string_out;
  }

  static TestGeometry FromWKT(std::string_view wkt) {
    TestGeometry result;
    result.label_ = wkt;

    struct GeoArrowStringView wkt_view{wkt.data(),
                                       static_cast<int64_t>(wkt.size())};

    struct GeoArrowVisitor v{};
    GeoArrowGeometryInitVisitor(&result.geom_, &v);

    struct GeoArrowWKTReader reader;
    GEOARROW_THROW_NOT_OK(nullptr, GeoArrowWKTReaderInit(&reader));
    GeoArrowErrorCode code = GeoArrowWKTReaderVisit(&reader, wkt_view, &v);
    GeoArrowWKTReaderReset(&reader);
    if (code != GEOARROW_OK) {
      throw std::runtime_error("Invalid WKT");
    }

    return result;
  }

  static TestGeometry FromWKB(std::vector<uint8_t> wkb) {
    TestGeometry result;
    result.data_ = std::move(wkb);

    struct GeoArrowWKBReader reader;
    GEOARROW_THROW_NOT_OK(nullptr, GeoArrowWKBReaderInit(&reader));
    struct GeoArrowBufferView src{result.data_.data(),
                                  static_cast<int64_t>(result.data_.size())};
    struct GeoArrowGeometryView view;
    GeoArrowErrorCode code =
        GeoArrowWKBReaderRead(&reader, src, &view, nullptr);
    if (code != GEOARROW_OK) {
      GeoArrowWKBReaderReset(&reader);
      throw std::runtime_error("Invalid WKB");
    }

    // Copy the parsed geometry into our owned GeoArrowGeometry. Because data_
    // is attached to this object, the pointed to buffers from the nodes will
    // stay valid
    code = GeoArrowGeometryShallowCopy(view, &result.geom_);
    GeoArrowWKBReaderReset(&reader);
    if (code != GEOARROW_OK) {
      throw std::runtime_error("Failed to copy WKB geometry");
    }

    return result;
  }

  struct GeoArrowGeometryView geom() const {
    return GeoArrowGeometryAsView(&geom_);
  }

  bool oriented() const { return oriented_; }

  void set_oriented(bool oriented) { oriented_ = oriented; }

  std::string_view label() const { return label_; }

 private:
  struct GeoArrowGeometry geom_;
  std::string label_;
  bool oriented_;
  std::vector<uint8_t> data_;
};

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
      geoarrow::Wkb()
          .WithEdgeType(GEOARROW_EDGE_TYPE_SPHERICAL)
          .InitSchema(schemas.back().get());
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
    struct ArrowArray* result, std::vector<std::optional<std::string>> expected,
    int precision = 6) {
  ASSERT_EQ(result->length, expected.size());

  nanoarrow::UniqueArrayView result_view;
  ArrowArrayViewInitFromType(result_view.get(), NANOARROW_TYPE_BINARY);
  NANOARROW_THROW_NOT_OK(
      ArrowArrayViewSetArray(result_view.get(), result, nullptr));

  for (int64_t i = 0; i < result->length; i++) {
    SCOPED_TRACE("expected[" + std::to_string(i) + "]");
    if (ArrowArrayViewIsNull(result_view.get(), i)) {
      ASSERT_FALSE(expected[i].has_value())
          << "Expected " << ::testing::PrintToString(*expected[i])
          << " but got NULL";
    } else {
      auto actual_binary = ArrowArrayViewGetBytesUnsafe(result_view.get(), i);
      std::vector<uint8_t> actual_binary_vec(
          actual_binary.data.as_uint8,
          actual_binary.data.as_uint8 + actual_binary.size_bytes);
      auto actual_geometry = TestGeometry::FromWKB(actual_binary_vec);

      ASSERT_TRUE(expected[i].has_value())
          << "Expected NULL but got "
          << ::testing::PrintToString(actual_geometry.ToWKT(precision));

      auto expected_geometry = TestGeometry::FromWKT(*expected[i]);
      ASSERT_EQ(actual_geometry.ToWKT(precision),
                expected_geometry.ToWKT(precision));
    }
  }
}
