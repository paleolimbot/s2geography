
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
///
/// The functions here are designed to have some common properties:
///
/// - Preconditions are only checked in debug mode (i.e., users are
///   responsible for checking that C API functions are passed
///   non-NULL inputs where required). This includes destructors.
/// - Output arguments of functions are only modified on success
/// - Pointers that are const (e.g., const S2Geog*) may be shared
///   between threads, like const std::vector (which is generally the
///   model also used for all S2 C++ objects).

/// \defgroup error_handling Error Handling
/// Functions for creating and managing error objects.
///
/// Most functions in this C API return an errno-compatible error code
/// (usually EINVAL or ENOTSUP) and optionally accept an err parameter
/// into which more detailed message is placed when given a non-NULL
/// value by the caller. This error object can and should be reused
/// between calls (e.g., users may wish to allocate a thread-local error
/// and reuse it).
///
/// Functions that can't fail do not return error codes or accept error
/// arguments and return their values directly.
///
/// @{

/// \brief An errno-compatible error code
typedef int S2GeogErrorCode;

/// \brief Value returned on success
#define S2GEOGRAPHY_OK 0

/// \brief Opaque error object containing error details
struct S2GeogError;

/// \brief Create a new error object
///
/// \pre err != NULL
S2GeogErrorCode S2GeogErrorCreate(struct S2GeogError** err);

/// \brief Get the error message from an error object
///
/// This message is always guaranteed to be a null-terminated
/// string (usually the empty string).
const char* S2GeogErrorGetMessage(const struct S2GeogError* err);

/// \brief Destroy an error object
///
/// \pre err != NULL
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
///
/// \pre vertex != NULL
uint64_t S2GeogLngLatToCellId(const struct S2GeogVertex* vertex);

/// @}

/// \defgroup geography_accessors Geography Accessors
/// Basic geography object creation and destruction.
///
/// @{

/// \brief Opaque geography object
struct S2Geog;

/// \brief Create an empty geography object
///
/// \pre geog != NULL
S2GeogErrorCode S2GeogCreate(struct S2Geog** geog);

/// \brief Force building the internal shape index for this geography
///
/// Most operations attempt to avoid building the internal ShapeIndex
/// attached to this geography because doing so can be slow; however, for
/// repeated calls to the same function (e.g., predicates), it can be faster to
/// force the creation of an index. For example, internal S2 logic generally
/// avoids building an index for loops with less than 32 vertices unless there
/// have been 20 or more point containment queries on the same loop. In
/// S2Geography, we leave the choice of whether or not to eagerly build an index
/// to the user.
///
/// \pre geog != NULL
S2GeogErrorCode S2GeogForcePrepare(struct S2Geog* geog,
                                   struct S2GeogError* err);

/// \brief Return the memory used by a geography object
///
/// This returns the total memory footprint including sizeof(S2Geog), internal
/// shape allocations, index memory, and owned coordinate buffers.
///
/// \pre geog != NULL
size_t S2GeogMemUsed(struct S2Geog* geog);

/// \brief Destroy a geography object
///
/// \pre geog != NULL
void S2GeogDestroy(struct S2Geog* geog);

/// @}

/// \defgroup geography_factory Geography Factory
/// Factory for creating geography objects from various formats.
///
/// The factory is fairly lightweight but should be reused when creating
/// many geographies in a batch.
/// @{

/// \brief Opaque factory for creating geography objects
struct S2GeogFactory;

/// \brief Create a new geography factory
///
/// \pre geog_factory != NULL
S2GeogErrorCode S2GeogFactoryCreate(struct S2GeogFactory** geog_factory);

/// \brief Create a geography from WKB without taking ownership of the buffer
///
/// The output S2Geog must have been created before this call with
/// S2GeogCreate(). This S2Geog can and should be reused for multiple calls to
/// this or other factory functions (geographies have internal scratch space
/// that can be reused).
///
/// \pre geog_factory != NULL
/// \pre out != NULL
/// \pre buf != NULL || buf_size == 0
S2GeogErrorCode S2GeogFactoryInitFromWkbNonOwning(
    struct S2GeogFactory* geog_factory, const uint8_t* buf, size_t buf_size,
    struct S2Geog* out, struct S2GeogError* err);

/// \brief Create a geography from WKT
///
/// The output S2Geog must have been created before this call with
/// S2GeogCreate(). The resulting geography owns its coordinates and
/// is not tied to the lifecycle of the input text. This is primarily
/// useful for testing.
///
/// This function may modify out in the case of error; however, is left in
/// a valid state and may be reused in another call to a factory method.
///
/// \pre geog_factory != NULL
/// \pre out != NULL
/// \pre buf != NULL || buf_size == 0
S2GeogErrorCode S2GeogFactoryInitFromWkt(struct S2GeogFactory* geog_factory,
                                         const char* buf, size_t buf_size,
                                         struct S2Geog* out,
                                         struct S2GeogError* err);

/// \brief Destroy a geography factory
///
/// \pre geog_factory != NULL
void S2GeogFactoryDestroy(struct S2GeogFactory* geog_factory);

/// @}

/// \defgroup bounding Rectangle bounding
/// Functions for computing a rectangular bounding area
///
/// @{

/// \brief Opaque rectangle bounder object
struct S2GeogRectBounder;

/// \brief Create a new rectangle bounder
///
/// \pre rect_bounder != NULL
S2GeogErrorCode S2GeogRectBounderCreate(
    struct S2GeogRectBounder** rect_bounder);

/// \brief Clear accumulated bounds from the rectangle bounder
///
/// \pre rect_bounder != NULL
void S2GeogRectBounderClear(struct S2GeogRectBounder* rect_bounder);

/// \brief Add a geography's bounds to the rectangle bounder
///
/// \pre rect_bounder != NULL
/// \pre geog != NULL
S2GeogErrorCode S2GeogRectBounderBound(struct S2GeogRectBounder* rect_bounder,
                                       const struct S2Geog* geog,
                                       struct S2GeogError* err);

/// \brief Expand the accumulated bounds by a distance
///
/// \pre rect_bounder != NULL
void S2GeogRectBounderExpandByDistance(struct S2GeogRectBounder* rect_bounder,
                                       double distance_meters);

/// \brief Return 1 if the rectangle that would be returned represents empty
/// bounds or 0 otherwise
///
/// \pre rect_bounder != NULL
uint8_t S2GeogRectBounderIsEmpty(struct S2GeogRectBounder* rect_bounder);

/// \brief Finish bounding and retrieve the lo/hi corners of the bounding
/// rectangle
///
/// \pre rect_bounder != NULL
/// \pre lo != NULL
/// \pre hi != NULL
S2GeogErrorCode S2GeogRectBounderFinish(struct S2GeogRectBounder* rect_bounder,
                                        struct S2GeogVertex* lo,
                                        struct S2GeogVertex* hi,
                                        struct S2GeogError* err);

/// \brief Destroy a rectangle bounder
///
/// \pre rect_bounder != NULL
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

/// \defgroup operations Operators
/// Generic interface for function operations
///
/// This interface exposes functions in a generic way that (1) allows for
/// operation-specific scratch space or expensive object construction to be
/// amortized over many calls and (2) reduces the number of functions required
/// in this C API. In general, only non-trivial functions are exposed in this
/// way because there is some overhead associated with this wrapping. Trivial
/// functions should be exposed directly in this C API or called via the Arrow
/// interface where these overheads can be more effectively amortized over
/// elements in a loop.
///
/// @{

/// \brief Opaque operator object
struct S2GeogOp;

/// \brief Compute the intersects predicate between two geographies, returning a
/// boolean
#define S2GEOGRAPHY_OP_INTERSECTS 1

/// \brief Compute the contains predicate between two geographies, returning a
/// boolean
#define S2GEOGRAPHY_OP_CONTAINS 2

/// \brief Compute the within predicate between two geographies, returning a
/// boolean
#define S2GEOGRAPHY_OP_WITHIN 3

/// \brief Compute the equals predicate between two geographies, returning a
/// boolean
#define S2GEOGRAPHY_OP_EQUALS 4

/// \brief Compute the distance-within predicate between two geographies with a
/// distance threshold, returning a boolean
#define S2GEOGRAPHY_OP_DISTANCE_WITHIN 5

#define S2GEOGRAPHY_OUTPUT_TYPE_BOOL 1

/// \brief Create a new operator object
///
/// \pre op != NULL
S2GeogErrorCode S2GeogOpCreate(struct S2GeogOp** op, int op_id);

/// \brief Get the name of the operator
///
/// \pre op != NULL
const char* S2GeogOpName(const struct S2GeogOp* op);

/// \brief Get the output type of the operator
///
/// \pre op != NULL
int S2GeogOpOutputType(const struct S2GeogOp* op);

/// \brief Evaluate an operation with two geographies as input
///
/// \pre op != NULL
/// \pre arg0 != NULL
/// \pre arg1 != NULL
S2GeogErrorCode S2GeogOpEvalGeogGeog(struct S2GeogOp* op, const S2Geog* arg0,
                                     const S2Geog* arg1,
                                     struct S2GeogError* err);

/// \brief Evaluate an operation with two geographies and a double as input
///
/// \pre op != NULL
/// \pre arg0 != NULL
/// \pre arg1 != NULL
S2GeogErrorCode S2GeogOpEvalGeogGeogDouble(struct S2GeogOp* op,
                                           const S2Geog* arg0,
                                           const S2Geog* arg1, double arg2,
                                           struct S2GeogError* err);

/// \brief Get integer or boolean output for this operation
///
/// \pre op != NULL
int64_t S2GeogOpGetInt(struct S2GeogOp* op);

/// \brief Destroy an op object
///
/// \pre op != NULL
void S2GeogOpDestroy(struct S2GeogOp* op);

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
