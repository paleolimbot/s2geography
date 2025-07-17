
#include "s2geography/arrow_udf/arrow_udf.h"

#include <gtest/gtest.h>

#include "geoarrow/geoarrow.hpp"
#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/geoarrow.h"
#include "s2geography/s2geography_gtest_util.h"

using s2geography::WktEquals6;

// We use a simple model for testing functions: types are either geoarrow.wkb
// or an Arrow type (the only ones used here are bool, int32, and double).
// We define some aliases here to ensure we can expand this if we need to and
// make it more clean in the tests what the input/output types actually are.
using ArrowTypeOrWKB = std::optional<enum ArrowType>;
#define ARROW_TYPE_WKB std::nullopt

// Create the ArrowSchema required to initialize an ArrowUDF
nanoarrow::UniqueSchema ArgSchema(std::vector<ArrowTypeOrWKB> cols) {
  nanoarrow::UniqueSchema schema;
  ArrowSchemaInit(schema.get());
  NANOARROW_THROW_NOT_OK(ArrowSchemaSetTypeStruct(schema.get(), cols.size()));
  for (int64_t i = 0; i < static_cast<int64_t>(cols.size()); i++) {
    if (cols[i]) {
      NANOARROW_THROW_NOT_OK(ArrowSchemaSetType(schema->children[i], *cols[i]));
    } else {
      ArrowSchemaRelease(schema->children[i]);
      geoarrow::Wkb().InitSchema(schema->children[i]);
    }
  }
  return schema;
}

// Create geoarrow.wkb argument from WKT
nanoarrow::UniqueArray ArgWkb(
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
nanoarrow::UniqueArray ArgArrow(enum ArrowType type,
                                std::vector<std::optional<double>> values) {
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

// Test utility to call udf->Init() and check its output type.
void TestInitArrowUDF(s2geography::arrow_udf::ArrowUDF* udf,
                      std::vector<ArrowTypeOrWKB> arg_types,
                      ArrowTypeOrWKB result_type) {
  auto arg_schema = ArgSchema(std::move(arg_types));
  nanoarrow::UniqueSchema result_schema;
  ASSERT_EQ(udf->Init(arg_schema.get(), "", result_schema.get()), NANOARROW_OK);

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

// Test utility to create argument arrays and pass them to udf->Execute()
// This exploits the property that all the functions we expose have geography
// arguments first.
void TestExecuteArrowUDF(
    s2geography::arrow_udf::ArrowUDF* udf,
    std::vector<ArrowTypeOrWKB> arg_types, ArrowTypeOrWKB result_type,
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

  ASSERT_EQ(udf->Execute(arg_pointers.data(),
                         static_cast<int64_t>(arg_pointers.size()), out),
            NANOARROW_OK);
}

// Check a non-geography result. Expected is an optional double here because
// we only expose functions whose return types are bool, int, or double
// (and all can be coerced to double).
void TestResultArrow(struct ArrowArray* result, enum ArrowType result_type,
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

  ASSERT_EQ(actual, expected);
}

// Check a geography result. This rounds the WKT output to 6 decimal places
// to avoid floating point differences between platforms.
void TestResultGeography(struct ArrowArray* result,
                         std::vector<std::optional<std::string>> expected) {
  ASSERT_EQ(result->length, expected.size());

  s2geography::geoarrow::Reader reader;
  reader.Init(s2geography::geoarrow::Reader::InputType::kWKB,
              s2geography::geoarrow::ImportOptions());
  std::vector<std::unique_ptr<s2geography::Geography>> geogs;
  reader.ReadGeography(result, 0, result->length, &geogs);

  for (int64_t i = 0; i < result->length; i++) {
    SCOPED_TRACE("expected[" + std::to_string(i) + "]");
    if (geogs[i].get() == nullptr) {
      ASSERT_FALSE(expected[i].has_value());
    } else {
      ASSERT_TRUE(expected[i].has_value());
      ASSERT_THAT(*geogs[i], WktEquals6(*expected[i]));
    }
  }
}

TEST(ArrowUdf, Length) {
  auto udf = s2geography::arrow_udf::Length();

  ASSERT_NO_FATAL_FAILURE(
      TestInitArrowUDF(udf.get(), {ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteArrowUDF(udf.get(), {ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE,
                          {{"POINT (0 1)", "LINESTRING (0 0, 0 1)",
                            "POLYGON ((0 0, 0 1, 1 0, 0 0))", std::nullopt}},
                          {}, out_array.get()));

  ASSERT_NO_FATAL_FAILURE(
      TestResultArrow(out_array.get(), NANOARROW_TYPE_DOUBLE,
                      {0.0, 111195.10117748393, 0.0, ARROW_TYPE_WKB}));
}

TEST(ArrowUdf, Centroid) {
  auto udf = s2geography::arrow_udf::Centroid();

  ASSERT_NO_FATAL_FAILURE(
      TestInitArrowUDF(udf.get(), {ARROW_TYPE_WKB}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteArrowUDF(udf.get(), {ARROW_TYPE_WKB}, NANOARROW_TYPE_DOUBLE,
                          {{"POINT (0 1)", "LINESTRING (0 0, 0 1)",
                            "POLYGON ((0 0, 0 1, 1 0, 0 0))", std::nullopt}},
                          {}, out_array.get()));

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(), {"POINT (0 1)", "POINT (0 0.5)",
                        "POINT (0.33335 0.333344)", ARROW_TYPE_WKB}));
}

TEST(ArrowUdf, InterpolateNormalized) {
  auto udf = s2geography::arrow_udf::InterpolateNormalized();

  ASSERT_NO_FATAL_FAILURE(TestInitArrowUDF(
      udf.get(), {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE}, ARROW_TYPE_WKB));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(
      TestExecuteArrowUDF(udf.get(), {ARROW_TYPE_WKB, NANOARROW_TYPE_DOUBLE},
                          ARROW_TYPE_WKB, {{"LINESTRING (0 0, 0 1)"}},
                          {{0.0, 0.5, 1.0, ARROW_TYPE_WKB}}, out_array.get()));

  ASSERT_NO_FATAL_FAILURE(TestResultGeography(
      out_array.get(),
      {"POINT (0 0)", "POINT (0 0.5)", "POINT (0 1)", ARROW_TYPE_WKB}));
}

TEST(ArrowUdf, Intersects) {
  auto udf = s2geography::arrow_udf::Intersects();

  ASSERT_NO_FATAL_FAILURE(TestInitArrowUDF(
      udf.get(), {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL));

  nanoarrow::UniqueArray out_array;
  ASSERT_NO_FATAL_FAILURE(TestExecuteArrowUDF(
      udf.get(), {ARROW_TYPE_WKB, ARROW_TYPE_WKB}, NANOARROW_TYPE_BOOL,
      {{"POLYGON ((0 0, 1 0, 0 1, 0 0))"},
       {"POINT (0.25 0.25)", "POINT (-1 -1)", std::nullopt}},
      {}, out_array.get()));

  ASSERT_NO_FATAL_FAILURE(TestResultArrow(out_array.get(), NANOARROW_TYPE_BOOL,
                                          {true, false, ARROW_TYPE_WKB}));
}
