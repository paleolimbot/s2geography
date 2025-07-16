
#include "s2geography/arrow_udf/arrow_udf.h"

#include <gtest/gtest.h>

#include "geoarrow/geoarrow.hpp"
#include "nanoarrow/nanoarrow.hpp"
#include "s2geography/geoarrow.h"
#include "s2geography/s2geography_gtest_util.h"

using s2geography::WktEquals6;

nanoarrow::UniqueSchema ArgSchema(
    std::vector<std::optional<enum ArrowType>> cols) {
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

nanoarrow::UniqueArray ArgWkb(const std::vector<std::string>& values) {
  // Make a WKB array
  nanoarrow::UniqueArray array;
  NANOARROW_THROW_NOT_OK(
      ArrowArrayInitFromType(array.get(), NANOARROW_TYPE_STRING));
  NANOARROW_THROW_NOT_OK(ArrowArrayStartAppending(array.get()));
  for (const auto& value : values) {
    if (value == "") {
      NANOARROW_THROW_NOT_OK(ArrowArrayAppendNull(array.get(), 1));
    } else {
      ArrowStringView na_value{value.data(),
                               static_cast<int64_t>(value.size())};
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

template <typename c_type>
nanoarrow::UniqueArray ArgArrow(enum ArrowType type,
                                std::vector<std::optional<c_type>> values) {
  nanoarrow::UniqueArray array;
  NANOARROW_THROW_NOT_OK(ArrowArrayInitFromType(array.get(), type));
  NANOARROW_THROW_NOT_OK(ArrowArrayStartAppending(array.get()));

  for (const auto value : values) {
    if (value.has_value()) {
      if constexpr (std::is_integral_v<c_type>) {
        NANOARROW_THROW_NOT_OK(ArrowArrayAppendInt(array.get(), *value));
      } else if constexpr (std::is_floating_point_v<c_type>) {
        NANOARROW_THROW_NOT_OK(ArrowArrayAppendDouble(array.get(), *value));
      } else {
        static_assert(false, "type not supported");
      }
    } else {
      NANOARROW_THROW_NOT_OK(ArrowArrayAppendNull(array.get(), 1));
    }
  }

  NANOARROW_THROW_NOT_OK(ArrowArrayFinishBuildingDefault(array.get(), nullptr));
  return array;
}

TEST(ArrowUdf, Length) {
  auto arg_schema = ArgSchema({std::nullopt});
  auto udf = s2geography::arrow_udf::Length();

  nanoarrow::UniqueSchema out_type;
  ASSERT_EQ(udf->Init(arg_schema.get(), "", out_type.get()), NANOARROW_OK);

  nanoarrow::UniqueArrayView view;
  ASSERT_EQ(ArrowArrayViewInitFromSchema(view.get(), out_type.get(), nullptr),
            NANOARROW_OK);
  ASSERT_EQ(view->storage_type, NANOARROW_TYPE_DOUBLE);

  nanoarrow::UniqueArray in_array =
      ArgWkb({"POINT (0 1)", "LINESTRING (0 0, 0 1)",
              "POLYGON ((0 0, 0 1, 1 0, 0 0))", ""});
  std::vector<struct ArrowArray*> args;
  args.push_back(in_array.get());
  nanoarrow::UniqueArray out_array;
  ASSERT_EQ(udf->Execute(args.data(), static_cast<int64_t>(args.size()),
                         out_array.get()),
            NANOARROW_OK);

  ASSERT_EQ(ArrowArrayViewSetArray(view.get(), out_array.get(), nullptr),
            NANOARROW_OK);
  ASSERT_EQ(view->length, 4);
  EXPECT_EQ(view->null_count, 1);
  EXPECT_EQ(ArrowArrayViewGetDoubleUnsafe(view.get(), 0), 0.0);
  EXPECT_DOUBLE_EQ(ArrowArrayViewGetDoubleUnsafe(view.get(), 1),
                   111195.10117748393);
  EXPECT_EQ(ArrowArrayViewGetDoubleUnsafe(view.get(), 2), 0.0);
  EXPECT_TRUE(ArrowArrayViewIsNull(view.get(), 3));
}

TEST(ArrowUdf, Centroid) {
  auto arg_schema = ArgSchema({std::nullopt});
  auto udf = s2geography::arrow_udf::Centroid();

  nanoarrow::UniqueSchema out_type;
  ASSERT_EQ(udf->Init(arg_schema.get(), "", out_type.get()), NANOARROW_OK);

  struct ArrowSchemaView out_type_view;
  ASSERT_EQ(ArrowSchemaViewInit(&out_type_view, out_type.get(), nullptr),
            NANOARROW_OK);
  ASSERT_EQ(out_type_view.type, NANOARROW_TYPE_BINARY);

  s2geography::geoarrow::Reader reader;
  reader.Init(s2geography::geoarrow::Reader::InputType::kWKB,
              s2geography::geoarrow::ImportOptions());

  nanoarrow::UniqueArray in_array =
      ArgWkb({"POINT (0 1)", "LINESTRING (0 0, 0 1)",
              "POLYGON ((0 0, 0 1, 1 0, 0 0))", ""});
  std::vector<struct ArrowArray*> args;
  args.push_back(in_array.get());
  nanoarrow::UniqueArray out_array;
  ASSERT_EQ(udf->Execute(args.data(), static_cast<int64_t>(args.size()),
                         out_array.get()),
            NANOARROW_OK);
  ASSERT_EQ(out_array->length, 4);

  std::vector<std::unique_ptr<s2geography::Geography>> result;
  reader.ReadGeography(out_array.get(), 0, out_array->length, &result);
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(*result[0], WktEquals6("POINT (0 1)"));
  EXPECT_THAT(*result[1], WktEquals6("POINT (0 0.5)"));
  EXPECT_THAT(*result[2], WktEquals6("POINT (0.33335 0.333344)"));
  EXPECT_EQ(result[3].get(), nullptr);
}

TEST(ArrowUdf, InterpolateNormalized) {
  auto arg_schema = ArgSchema({std::nullopt, NANOARROW_TYPE_DOUBLE});
  auto udf = s2geography::arrow_udf::InterpolateNormalized();

  nanoarrow::UniqueSchema out_type;
  ASSERT_EQ(udf->Init(arg_schema.get(), "", out_type.get()), NANOARROW_OK);

  struct ArrowSchemaView out_type_view;
  ASSERT_EQ(ArrowSchemaViewInit(&out_type_view, out_type.get(), nullptr),
            NANOARROW_OK);
  ASSERT_EQ(out_type_view.type, NANOARROW_TYPE_BINARY);

  s2geography::geoarrow::Reader reader;
  reader.Init(s2geography::geoarrow::Reader::InputType::kWKB,
              s2geography::geoarrow::ImportOptions());

  nanoarrow::UniqueArray in_array0 = ArgWkb({"LINESTRING (0 0, 0 1)"});
  nanoarrow::UniqueArray in_array1 =
      ArgArrow<double>(NANOARROW_TYPE_DOUBLE, {0.0, 0.5, 1.0, std::nullopt});

  std::vector<struct ArrowArray*> args = {in_array0.get(), in_array1.get()};
  nanoarrow::UniqueArray out_array;
  ASSERT_EQ(udf->Execute(args.data(), static_cast<int64_t>(args.size()),
                         out_array.get()),
            NANOARROW_OK);
  ASSERT_EQ(out_array->length, 4);

  std::vector<std::unique_ptr<s2geography::Geography>> result;
  reader.ReadGeography(out_array.get(), 0, out_array->length, &result);
  ASSERT_EQ(result.size(), 4);
  EXPECT_THAT(*result[0], WktEquals6("POINT (0 0)"));
  EXPECT_THAT(*result[1], WktEquals6("POINT (0 0.5)"));
  EXPECT_THAT(*result[2], WktEquals6("POINT (0 1)"));
  EXPECT_EQ(result[3].get(), nullptr);
}
