
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "s2geography/geoarrow-geography.h"
#include "s2geography/geography_interface.h"
#include "s2geography/macros.h"

namespace s2geography {

class Operation {
 public:
  enum class OutputType { kBool, kDouble, kWkb };

  Operation() = default;
  virtual ~Operation() = default;

  virtual const std::string& name() const = 0;
  virtual OutputType output_type() const = 0;

  virtual void ExecGeogGeog(const GeoArrowGeography& arg0,
                            const GeoArrowGeography& arg1) {
    S2GEOGRAPHY_UNUSED(arg0);
    S2GEOGRAPHY_UNUSED(arg1);
    throw Exception("Can't call " + name() + " with geog + geog");
  }

  bool has_result() const { return has_result_; }
  int64_t GetInt() const { return int_result_; }
  double GetDouble() const { return double_result_; }
  std::string_view GetStringView() const { return string_result_; }

 protected:
  bool has_result_{true};
  int64_t int_result_{};
  double double_result_{};
  std::string_view string_result_{};
};

}  // namespace s2geography
