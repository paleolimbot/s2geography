
#include "s2geography/wkb.h"

#include <cstdlib>
#include <cstring>
#include <sstream>

#include "s2geography/geoarrow.h"
#include "s2geography/geography.h"

namespace s2geography {

WKBReader::WKBReader(const geoarrow::ImportOptions& options) {
  reader_ = absl::make_unique<geoarrow::Reader>();
  reader_->Init(geoarrow::Reader::InputType::kWKB, options);
}

std::unique_ptr<Geography> WKBReader::ReadFeature(const uint8_t* bytes,
                                                   int64_t size) {
  if (size > std::numeric_limits<int32_t>::max()) {
    throw Exception("Can't parse WKB greater than 2GB in size");
  }

  int32_t offsets[] = {0, static_cast<int32_t>(size)};
  const void* buffers[] = {nullptr, offsets, bytes};
  ArrowArray array;
  array.length = 1;
  array.null_count = 0;
  array.offset = 0;

  array.n_buffers = 3;
  array.n_children = 0;
  array.buffers = buffers;

  array.children = nullptr;
  array.dictionary = nullptr;
  array.release = [](ArrowArray*) -> void {};
  array.private_data = nullptr;

  out_.clear();
  reader_->ReadGeography(&array, 0, 1, &out_);
  return std::move(out_[0]);
}

std::unique_ptr<Geography> WKBReader::ReadFeature(const std::basic_string<uint8_t>& bytes) {
  return ReadFeature(bytes.data(), bytes.size());
}

WKBWriter::WKBWriter(const geoarrow::ExportOptions& options) {
  writer_ = absl::make_unique<geoarrow::Writer>();
  writer_->Init(geoarrow::Writer::OutputType::kWKB, options);

}

std::basic_string<uint8_t> WKBWriter::WriteFeature(const Geography& geog) {
  ArrowArray array;
  writer_->WriteGeography(geog);
  writer_->Finish(&array);

  const auto offsets = static_cast<const int32_t*>(array.buffers[1]);
  const auto data = static_cast<const uint8_t*>(array.buffers[2]);
  std::basic_string<uint8_t> result(data, offsets[1]);

  return result;
}

}  // namespace s2geography
