
#pragma once

#include <memory>
#include <sstream>

#include "s2geography/geoarrow.h"
#include "s2geography/geography.h"

namespace s2geography {

class WKTWriter {
 public:
  WKTWriter();
  WKTWriter(int precision);
  WKTWriter(const geoarrow::ExportOptions& options);

  std::string write_feature(const Geography& geog);

 private:
  std::unique_ptr<geoarrow::Writer> writer_;
};

}  // namespace s2geography
