
#include "s2geography/wkt-reader.h"

#include <cstdlib>
#include <cstring>
#include <sstream>

#include "s2geography/geoarrow.h"
#include "s2geography/geography.h"

namespace s2geography {

WKTReader::WKTReader(const geoarrow::ImportOptions& options) {
  reader_ = absl::make_unique<geoarrow::Reader>();
  reader_->Init(geoarrow::Reader::InputType::kWKT, options);
}

std::unique_ptr<Geography> WKTReader::read_feature(const char* text,
                                                   int64_t size) {
  if (size > std::numeric_limits<int32_t>::max()) {
    throw Exception("Can't parse WKT greater than 2GB in size");
  }

  int32_t offsets[] = {0, static_cast<int32_t>(size)};
  const void* buffers[] = {nullptr, offsets, text};
  ArrowArray array{.length = 1,
                   .offset = 0,
                   .null_count = 0,
                   .n_buffers = 3,
                   .buffers = buffers,
                   .n_children = 0,
                   .children = nullptr,
                   .dictionary = nullptr,
                   .release = [](ArrowArray*) -> void {}};

  out_.clear();
  reader_->ReadGeography(&array, 0, 1, &out_);
  return std::move(out_[0]);
}

std::unique_ptr<Geography> WKTReader::read_feature(const char* text) {
  return read_feature(text, strlen(text));
}

std::unique_ptr<Geography> WKTReader::read_feature(const std::string& str) {
  return read_feature(str.data(), str.size());
}

}  // namespace s2geography
