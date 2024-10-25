
#include "s2geography/wkt-writer.h"

#include "s2geography/accessors.h"
#include "s2geography/geoarrow.h"
#include "geoarrow/geoarrow.h"

namespace s2geography {

WKTWriter::WKTWriter() : WKTWriter(16) {}

WKTWriter::WKTWriter(int significant_digits) {
  geoarrow::ExportOptions options;
  options.set_significant_digits(significant_digits);

  writer_ = absl::make_unique<geoarrow::Writer>();
  struct ArrowSchema schema;
  GeoArrowSchemaInitExtension(&schema, GeoArrowType::GEOARROW_TYPE_WKT);
  writer_->Init(&schema, options);
}

WKTWriter::WKTWriter(const geoarrow::ExportOptions& options) {
  writer_ = absl::make_unique<geoarrow::Writer>();
  struct ArrowSchema schema;
  GeoArrowSchemaInitExtension(&schema, GeoArrowType::GEOARROW_TYPE_WKT);
  writer_->Init(&schema, options);
}

std::string WKTWriter::write_feature(const Geography& geog) {
  ArrowArray array;
  writer_->WriteGeography(geog);
  writer_->Finish(&array);

  const auto offsets = static_cast<const int32_t*>(array.buffers[1]);
  const auto data = static_cast<const char*>(array.buffers[2]);
  std::string result(data, offsets[1]);

  return result;
}

}  // namespace s2geography
