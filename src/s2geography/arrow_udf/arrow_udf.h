#pragma once

#include <cerrno>
#include <memory>

#include "s2geography/arrow_abi.h"

namespace s2geography {

namespace arrow_udf {

/// \brief C-friendly user-defined scalar function abstraction
///
/// Provides an abstract base class that can be used to export scalar
/// user-defined functions over an FFI boundary.
///
/// This object is NOT thread safe: callers must take care to serialize
/// access to methods if an instance is shared across threads. In general,
/// constructing and initializing this structure should be sufficiently
/// cheap that it shouldn't need to be shared in this way.
///
/// Implementations must not throw exceptions (i.e., must communicate
/// error information via errno and GetLastError()).
class ArrowUDF {
 public:
  virtual ~ArrowUDF() {}

  /// \brief Initialize the state of this UDF instance and calculate a return
  /// type
  ///
  /// \param arg_schema An ArrowSchema whose children define the arguments that
  /// will be passed. The udf MAY take ownership over arg_schema but does not
  /// have to (i.e., it is the caller's responsibility to release it if the
  /// release callback is non-null).
  /// \param options Serialized key-value pairs encoded in the same way as in
  /// the ArrowSchema::metadata field. These can be used to set options for a
  /// UDF that are always passed as constants.
  /// \param out Will be populated with the return type on success.
  ///
  /// \return An errno-compatible error code, or zero on success.
  virtual int Init(struct ArrowSchema* arg_schema, const char* options,
                   struct ArrowSchema* out) = 0;

  /// \brief Execute a single batch
  ///
  /// \param args Input arguments. Input must be length one (e.g., a scalar)
  /// or the size of the batch. Implementations must handle scalar or array
  /// inputs.
  /// \param n_args The number of pointers in args
  /// \param out Will be populated with the result on success.
  virtual int Execute(struct ArrowArray** args, int64_t n_args,
                      struct ArrowArray* out) = 0;

  /// \brief Get the last error message
  ///
  /// The result is valid until the next call to a UDF method.
  virtual const char* GetLastError() = 0;
};

/// \brief Instantiate an ArrowUDF for the s2_length() function
///
/// This ArrowUDF handles any GeoArrow array as input and produces
/// a double array as output. Note that unlike s2_length(), this
/// function returns results in meters by default.
std::unique_ptr<ArrowUDF> Length();

/// \brief Instantiate an ArrowUDF for the s2_centroid() function
///
/// This ArrowUDF handles any GeoArrow array as input and produces
/// a geoarrow.wkb array as output.
std::unique_ptr<ArrowUDF> Centroid();

/// \brief Instantiate an ArrowUDF for the s2_interpolate_normalized() function
///
/// This ArrowUDF accepts any GeoArrow array and any numeric array as input
/// and produces a boolean array as output.
std::unique_ptr<ArrowUDF> InterpolateNormalized();

/// \brief Instantiate an ArrowUDF for the s2_intersects() function
///
/// This ArrowUDF handles any GeoArrow array as input and produces a boolean
/// array as output.
std::unique_ptr<ArrowUDF> Intersects();

}  // namespace arrow_udf

}  // namespace s2geography
