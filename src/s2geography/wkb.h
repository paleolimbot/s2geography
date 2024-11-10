
#pragma once

#include "s2geography/geoarrow.h"
#include "s2geography/geography.h"

namespace s2geography {

class WKBReader {
 public:
  WKBReader() : WKBReader(geoarrow::ImportOptions()) {}
  WKBReader(const geoarrow::ImportOptions& options);
  std::unique_ptr<Geography> ReadFeature(const uint8_t* bytes, int64_t size);
  std::unique_ptr<Geography> ReadFeature(const std::basic_string_view<uint8_t> bytes);
  std::unique_ptr<Geography> ReadFeature(const std::string_view bytes);

 private:
  std::unique_ptr<geoarrow::Reader> reader_;
  std::vector<std::unique_ptr<Geography>> out_;
};

class WKBWriter {
 public:
  WKBWriter() : WKBWriter(geoarrow::ExportOptions()) {}
  WKBWriter(const geoarrow::ExportOptions& options);

  std::basic_string<uint8_t> WriteFeature(const Geography& geog);

 private:
  std::unique_ptr<geoarrow::Writer> writer_;
};

}  // namespace s2geography
