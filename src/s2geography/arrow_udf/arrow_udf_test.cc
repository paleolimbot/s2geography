
#include "s2geography/arrow_udf/arrow_udf.h"

#include <gtest/gtest.h>

#include "geoarrow/geoarrow.hpp"
#include "nanoarrow/nanoarrow.hpp"

nanoarrow::UniqueSchema ArgSchemaWkb() {
  nanoarrow::UniqueSchema schema;
  ArrowSchemaInit(schema.get());
  NANOARROW_THROW_NOT_OK(ArrowSchemaSetTypeStruct(schema.get(), 0));
  NANOARROW_THROW_NOT_OK(ArrowSchemaAllocateChildren(schema.get(), 1));
  geoarrow::Wkb()
      .WithEdgeType(GEOARROW_EDGE_TYPE_SPHERICAL)
      .InitSchema(schema->children[0]);

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

TEST(ArrowUdf, Length) {
  auto arg_schema = ArgSchemaWkb();
  auto udf = s2geography::arrow_udf::Length();

  nanoarrow::UniqueSchema out_type;
  ASSERT_EQ(udf->Init(arg_schema.get(), "", out_type.get()), NANOARROW_OK);

  nanoarrow::UniqueArrayView view;
  ASSERT_EQ(ArrowArrayViewInitFromSchema(view.get(), out_type.get(), nullptr),
            NANOARROW_OK);

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
