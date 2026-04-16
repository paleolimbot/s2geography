
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// \file s2geography_c.h
///
/// This file exposes C functions and/or data structures used to call
/// s2geography from C or languages that provide C FFI infrastructure
/// such as Rust or Julia.

/// \defgroup error_handling Error Handling
/// Functions for creating and managing error objects.
///
/// @{

/// \brief An errno-compatible error code
typedef int S2GeogErrorCode;

/// \brief Value returned on success
#define S2GEOGRAPHY_OK 0

/// \brief Opaque error object containing error details
struct S2GeogError;

/// \brief Create a new error object
S2GeogErrorCode S2GeogErrorCreate(struct S2GeogError** err);

/// \brief Get the error message from an error object
const char* S2GeogErrorGetMessage(struct S2GeogError* err);

/// \brief Destroy an error object
void S2GeogErrorDestroy(struct S2GeogError* err);

/// @}

/// \defgroup cell_functions Cell Functions
/// Functions for working with S2 cell identifiers.
///
/// @{

/// \brief Vertex used as common input/output for vertices
struct S2GeogVertex {
  double v[4];
};

/// \brief Convert vertex of longitude/latitude to an S2 cell ID
uint64_t S2GeogLngLatToCellId(struct S2GeogVertex* vertex);

/// @}

/// \defgroup geography_accessors Geography Accessors
/// Basic geography object creation and destruction.
///
/// @{

/// \brief Opaque geography object
struct S2Geog;

/// \brief Create an empty geography object
S2GeogErrorCode S2GeogCreate(struct S2Geog** geog);

/// \brief Destroy a geography object
void S2GeogDestroy(struct S2Geog* geog);

/// @}

/// \defgroup geography_factory Geography Factory
/// Factory for creating geography objects from various formats.
///
/// @{

/// \brief Opaque factory for creating geography objects
struct S2GeogFactory;

/// \brief Create a new geography factory
S2GeogErrorCode S2GeogFactoryCreate(struct S2GeogFactory** geog_factory);

/// \brief Create a geography from WKB without taking ownership of the buffer
S2GeogErrorCode S2GeogFactoryInitFromWkbNonOwning(
    struct S2GeogFactory* geog_factory, const uint8_t* buf, size_t buf_size,
    struct S2Geog* out, struct S2GeogError* err);

/// \brief Destroy a geography factory
void S2GeogFactoryDestroy(struct S2GeogFactory* geog_factory);

/// @}

/// \defgroup bounding Rectangle bounding
/// Functions for computing a rectangular bounding area
///
/// @{

/// \brief Opaque rectangle bounder object
struct S2GeogRectBounder;

/// \brief Create a new rectangle bounder
S2GeogErrorCode S2GeogRectBounderCreate(
    struct S2GeogRectBounder** rect_bounder);

/// \brief Clear accumulated bounds from the rectangle bounder
void S2GeogRectBounderClear(struct S2GeogRectBounder* rect_bounder);

/// \brief Add a geography's bounds to the rectangle bounder
S2GeogErrorCode S2GeogRectBounderBound(struct S2GeogRectBounder* rect_bounder,
                                       struct S2Geog* geog,
                                       struct S2GeogError* err);

/// \brief Return 1 if the rectangle that would be returned represents empty
/// bounds or 0 otherwise
uint8_t S2GeogRectBounderIsEmpty(struct S2GeogRectBounder* rect_bounder);

/// \brief Finish bounding and retrieve the lo/hi corners of the bounding
/// rectangle
S2GeogErrorCode S2GeogRectBounderFinish(struct S2GeogRectBounder* rect_bounder,
                                        struct S2GeogVertex* lo,
                                        struct S2GeogVertex* hi,
                                        struct S2GeogError* err);

/// \brief Destroy a rectangle bounder
void S2GeogRectBounderDestroy(struct S2GeogRectBounder* rect_bounder);

/// @}

/// \defgroup sedona_udf Sedona UDF Interface
///
/// Interface for user-defined functions. These functions take Arrow as input
/// and produce Arrow as output and are useful as a generic interface to avoid
/// verbose wrappers around S2 functions. Consuming S2Geography in this way has
/// the disadvantage that preparatory work (e.g., preparing a geography) is not
/// effectively reused between calls.
///
/// @{

/// \brief Kernel format for Apache Sedona's scalar UDF extension
#define S2GEOGRAPHY_KERNEL_FORMAT_SEDONA_UDF 1

/// \brief The number of user-defined functions to be exported
size_t S2GeogNumKernels(void);

/// \brief Export functions into an array of the appropriate type
///
/// The only currently supported format is S2GEOGRAPHY_KERNEL_FORMAT_SEDONA_UDF.
S2GeogErrorCode S2GeogInitKernels(void* kernels_array,
                                  size_t kernels_array_size_bytes, int format);

/// @}

/// \defgroup versions Version Information
/// Functions for retrieving version information of dependencies.
///
/// @{

/// \brief Get the nanoarrow library version
const char* S2GeogNanoarrowVersion(void);

/// \brief Get the GeoArrow library version
const char* S2GeogGeoArrowVersion(void);

/// \brief Get the OpenSSL library version
const char* S2GeogOpenSSLVersion(void);

/// \brief Get the S2Geometry library version
const char* S2GeogS2GeometryVersion(void);

/// \brief Get the Abseil library version
const char* S2GeogAbseilVersion(void);

/// @}

#ifdef __cplusplus
}
#endif
