
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "s2geography/geoarrow-geography.h"
#include "s2geography/geography_interface.h"
#include "s2geography/macros.h"

namespace s2geography {

/// \brief Generic abstract operator
///
/// This class is an abstract base for non-trivial operations (i.e., those
/// expensive enough that the cost of the abstract dispatch is trivial compared
/// to the operation in question). This is intended to (1) make it easier to
/// wrap operations of s2geography and (2) ensure that operations that benefit
/// from a cache of scratch space or heavy object initialization can amortize
/// that overhead over many evaluations of a single function. Operations that
/// are more sensitive to this type of overhead should expose the functionality
/// directly or use the SedonaUDF interface.
///
/// Note that this operation does not participate in the propagation of nulls
/// from inputs but can represent null outputs for operations that can return
/// null even for some non-null inputs.
class Operation {
 public:
  /// \brief Output type enumerator
  enum class OutputType { kBool, kInt, kDouble, kWkb };

  Operation() = default;
  virtual ~Operation() = default;

  /// \brief The name of this operation (e.g., for errors, debugging, or generic
  /// wrapping)
  virtual const std::string& name() const = 0;

  /// \brief The output type of this operation (e.g., for generic wrapping when
  /// an output object type must be chosen)
  virtual OutputType output_type() const = 0;

  /// \brief Execute a function with two geographies as input
  virtual void ExecGeogGeog(const GeoArrowGeography& arg0,
                            const GeoArrowGeography& arg1) {
    S2GEOGRAPHY_UNUSED(arg0);
    S2GEOGRAPHY_UNUSED(arg1);
    throw Exception("Can't call " + name() + " with geog + geog");
  }

  /// \brief Execute a function with two geographies and a double as input
  virtual void ExecGeogGeogDouble(const GeoArrowGeography& arg0,
                                  const GeoArrowGeography& arg1, double arg2) {
    S2GEOGRAPHY_UNUSED(arg0);
    S2GEOGRAPHY_UNUSED(arg1);
    S2GEOGRAPHY_UNUSED(arg2);
    throw Exception("Can't call " + name() + " with geog + geog + double");
  }

  /// \brief Return true if the output of the last Exec call is non-null
  ///
  /// This is needed for functions that can return null even for non-null
  /// inputs.
  bool has_result() const { return has_result_; }

  /// \brief Return the integer result for operations whose output type is kInt
  /// or kBool
  ///
  /// The result before a call to an Exec or for a function with a different
  /// output type is not defined.
  int64_t GetInt() const { return int_result_; }

  /// \brief Return the double result for operations whose output type is
  /// kDouble
  ///
  /// The result before a call to an Exec or for a function with a different
  /// output type is not defined.
  double GetDouble() const { return double_result_; }

  /// \brief Return the byte array result for operations whose output type is
  /// kWkb
  ///
  /// The result before a call to an Exec or for a function with a different
  /// output type is not defined.
  std::string_view GetStringView() const { return string_result_; }

 protected:
  bool has_result_{true};
  int64_t int_result_{};
  double double_result_{};
  std::string_view string_result_{};
};

}  // namespace s2geography
