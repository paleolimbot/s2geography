
// Note: no #pragma once here...this header uses the Arrow
// canonical guard to prevent being included twice.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Extra guard for versions of Arrow without the canonical guard
#ifndef ARROW_FLAG_DICTIONARY_ORDERED

#ifndef ARROW_C_DATA_INTERFACE
#define ARROW_C_DATA_INTERFACE

#define ARROW_FLAG_DICTIONARY_ORDERED 1
#define ARROW_FLAG_NULLABLE 2
#define ARROW_FLAG_MAP_KEYS_SORTED 4

struct ArrowSchema {
  // Array type description
  const char* format;
  const char* name;
  const char* metadata;
  int64_t flags;
  int64_t n_children;
  struct ArrowSchema** children;
  struct ArrowSchema* dictionary;

  // Release callback
  void (*release)(struct ArrowSchema*);
  // Opaque producer-specific data
  void* private_data;
};

struct ArrowArray {
  // Array data description
  int64_t length;
  int64_t null_count;
  int64_t offset;
  int64_t n_buffers;
  int64_t n_children;
  const void** buffers;
  struct ArrowArray** children;
  struct ArrowArray* dictionary;

  // Release callback
  void (*release)(struct ArrowArray*);
  // Opaque producer-specific data
  void* private_data;
};

#endif  // ARROW_C_DATA_INTERFACE

#ifndef ARROW_C_STREAM_INTERFACE
#define ARROW_C_STREAM_INTERFACE

struct ArrowArrayStream {
  // Callback to get the stream type
  // (will be the same for all arrays in the stream).
  //
  // Return value: 0 if successful, an `errno`-compatible error code otherwise.
  //
  // If successful, the ArrowSchema must be released independently from the
  // stream.
  int (*get_schema)(struct ArrowArrayStream*, struct ArrowSchema* out);

  // Callback to get the next array
  // (if no error and the array is released, the stream has ended)
  //
  // Return value: 0 if successful, an `errno`-compatible error code otherwise.
  //
  // If successful, the ArrowArray must be released independently from the
  // stream.
  int (*get_next)(struct ArrowArrayStream*, struct ArrowArray* out);

  // Callback to get optional detailed error information.
  // This must only be called if the last stream operation failed
  // with a non-0 return code.
  //
  // Return value: pointer to a null-terminated character array describing
  // the last error, or NULL if no description is available.
  //
  // The returned pointer is only valid until the next operation on this stream
  // (including release).
  const char* (*get_last_error)(struct ArrowArrayStream*);

  // Release callback: release the stream's own resources.
  // Note that arrays returned by `get_next` must be individually released.
  void (*release)(struct ArrowArrayStream*);

  // Opaque producer-specific data
  void* private_data;
};

#endif  // ARROW_C_STREAM_INTERFACE
#endif  // ARROW_FLAG_DICTIONARY_ORDERED

#ifndef GEOARROW_C_ABI
#define GEOARROW_C_ABI

/// \brief Geometry type identifiers supported by GeoArrow
/// \ingroup geoarrow-schema
///
/// The values of this enum are intentionally chosen to be equivalent to
/// well-known binary type identifiers.
enum GeoArrowGeometryType {
  GEOARROW_GEOMETRY_TYPE_GEOMETRY = 0,
  GEOARROW_GEOMETRY_TYPE_POINT = 1,
  GEOARROW_GEOMETRY_TYPE_LINESTRING = 2,
  GEOARROW_GEOMETRY_TYPE_POLYGON = 3,
  GEOARROW_GEOMETRY_TYPE_MULTIPOINT = 4,
  GEOARROW_GEOMETRY_TYPE_MULTILINESTRING = 5,
  GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON = 6,
  GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION = 7,
  GEOARROW_GEOMETRY_TYPE_BOX = 990
};

/// \brief Dimension combinations supported by GeoArrow
/// \ingroup geoarrow-schema
enum GeoArrowDimensions {
  GEOARROW_DIMENSIONS_UNKNOWN = 0,
  GEOARROW_DIMENSIONS_XY = 1,
  GEOARROW_DIMENSIONS_XYZ = 2,
  GEOARROW_DIMENSIONS_XYM = 3,
  GEOARROW_DIMENSIONS_XYZM = 4
};

/// \brief Coordinate types supported by GeoArrow
/// \ingroup geoarrow-schema
enum GeoArrowCoordType {
  GEOARROW_COORD_TYPE_UNKNOWN = 0,
  GEOARROW_COORD_TYPE_SEPARATE = 1,
  GEOARROW_COORD_TYPE_INTERLEAVED = 2
};

/// \brief Edge types/interpolations supported by GeoArrow
/// \ingroup geoarrow-schema
enum GeoArrowEdgeType {
  GEOARROW_EDGE_TYPE_PLANAR,
  GEOARROW_EDGE_TYPE_SPHERICAL,
  GEOARROW_EDGE_TYPE_VINCENTY,
  GEOARROW_EDGE_TYPE_THOMAS,
  GEOARROW_EDGE_TYPE_ANDOYER,
  GEOARROW_EDGE_TYPE_KARNEY
};

/// \brief Coordinate reference system types supported by GeoArrow
/// \ingroup geoarrow-schema
enum GeoArrowCrsType {
  GEOARROW_CRS_TYPE_NONE,
  GEOARROW_CRS_TYPE_UNKNOWN,
  GEOARROW_CRS_TYPE_PROJJSON,
  GEOARROW_CRS_TYPE_WKT2_2019,
  GEOARROW_CRS_TYPE_AUTHORITY_CODE,
  GEOARROW_CRS_TYPE_SRID
};

/// \brief Flag to indicate that coordinates must be endian-swapped before being
/// interpreted on the current platform
#define GEOARROW_GEOMETRY_NODE_FLAG_SWAP_ENDIAN 0x01

/// \brief Generic Geometry node representation
///
/// This structure represents a generic view on a geometry, inspired by
/// DuckDB-spatial's sgl::geometry. The ownership of this struct is typically
/// managed by a GeoArrowGeometryRoot or a generic sequence type (e.g.,
/// std::vector). Its design allows for efficient iteration over a wide variety
/// of underlying structures without the need for recursive structures (but
/// allowing for recursive iteration where required).
///
/// A typical geometry is represented by one or more GeoArrowGeometryNodes
/// arranged sequentially in memory depth-first such that a node is followed by
/// its children (if any), then any remaining siblings from a common parent (if
/// any), and so on. Nodes should be passed by pointer such that a function can
/// iterate over children; however, function signatures should make this
/// expectation clear.
///
/// This structure is packed such that it is pointer-aligned and occupies 64
/// bytes.
struct GeoArrowGeometryNode {
  /// \brief Coordinate data
  ///
  /// Each pointer in coords points to the first coordinate in the sequence when
  /// geometry_type is GEOARROW_GEOMETRY_TYPE_POINT or
  /// GEOARROW_GEOMETRY_TYPE_LINESTRING ordered according to the dimensions
  /// specified in dimensions.
  ///
  /// The pointers need not be aligned. The data type must be a float, double,
  /// or signed 32-bit integer, communicated by a parent structure. Producers
  /// should produce double coordinates unless absolutely necessary; consumers
  /// may choose to only support double coordinates. Unless specified by a
  /// parent structure coordinates are C doubles.
  ///
  /// For dimension j, it must be safe to access the range
  /// [coords[j], coords[j] + size * stride[j] + sizeof(T)].
  ///
  /// The pointers in coords must never be NULL. Empty dimensions must point to
  /// a valid address whose value is NaN (for floating point types) or the most
  /// negative possible value (for integer types). This is true even when size
  /// is 0 (i.e., it must always be safe to access at least one value).
  const uint8_t* coords[4];

  /// \brief Number of bytes between adjacent coordinate values in coords,
  /// respectively
  ///
  /// The number of bytes to advance each pointer in coords when moving to the
  /// next coordinate. This allow representing a wide variety of coordinate
  /// layouts:
  ///
  /// - Interleaved coordinates: coord_stride is n_dimensions * sizeof(T). For
  ///   example, interleaved XY coordinates with double precision would have
  ///   coords set to {data, data + 8, &kNaN, &kNaN} and coord_stride set to
  ///   {16, 16, 0, 0}
  /// - Separated coordinates: coord_stride is sizeof(T). For example, separated
  ///   XY coordinates would have coords set to {x, y, &kNaN, &kNaN} and
  ///   coord_stride set to {8, 8, 0, 0}.
  /// - A constant value: coord_stride is 0. For example, the value 30, 10 as
  ///   a constant would have coords set to {&thirty, &ten, &kNaN, &kNaN} and
  ///   coord_stride set to {0, 0, 0, 0}.
  /// - WKB values with constant length packed end-to-end contiguously in memory
  ///   (e.g., in an Arrow array as the data buffer in an Array that does not
  ///   contain nulls): coord_stride is the size of one WKB item. For example,
  ///   an Arrow array of XY points would have coords set to {data + 1 + 4,
  ///   data + 1 + 4 + 8, &kNaN, &kNaN} and stride set to {21, 21, 0, 0}.
  /// - Any of the above but reversed (by pointing to the last coordinate
  ///   and setting the stride to a negative value).
  int32_t coord_stride[4];

  /// \brief The number of coordinates or children in this geometry
  ///
  /// When geometry_type is GEOARROW_GEOMETRY_TYPE_POINT or
  /// GEOARROW_GEOMETRY_TYPE_LINESTRING, the number of coordinates in the
  /// sequence. Otherwise, the number of child geometries.
  uint32_t size;

  /// \brief The GeoArrowGeometryType of this geometry
  ///
  /// For the purposes of this structure, rings of a polygon are considered a
  /// GEOARROW_GEOMETRY_TYPE_LINESTRING. The value
  /// GEOARROW_GEOMETRY_TYPE_UNINITIALIZED can be used to communicate an invalid
  /// or null value but must set size to zero.
  uint8_t geometry_type;

  /// \brief The GeoArrowDimensions
  uint8_t dimensions;

  /// \brief Flags
  ///
  /// The only currently supported flag is
  /// GEOARROW_GEOMETRY_NODE_FLAG_SWAP_ENDIAN to indicate that coords must be
  /// endian-swapped before being interpreted on the current platform.
  uint8_t flags;

  /// \brief The recursion level
  ///
  /// A level of 0 represents the root geometry and is incremented for
  /// child geometries (e.g., polygon ring or child of a multi geometry
  /// or collection).
  uint8_t level;

  /// \brief User data
  ///
  /// The user data is an opportunity for the producer to attach additional
  /// information to a node or for the consumer to cache information during
  /// processing. The producer nor the consumer must not rely on the value of
  /// this pointer for memory management (i.e., bookkeeping details must be
  /// handled elsewhere).
  const void* user_data;
};

/// \brief View of a geometry represented by a sequence of GeoArrowGeometryNode
///
/// This struct owns neither the array of nodes nor the array(s) of coordinates.
struct GeoArrowGeometryView {
  /// \brief A pointer to the root geometry node
  ///
  /// The memory is managed by the producer of the struct (e.g., a WKBReader
  /// will hold the array of GeoArrowGeometryNode and populate this struct
  /// to communicate the result.
  const struct GeoArrowGeometryNode* root;

  /// \brief The number of valid nodes in the root array
  ///
  /// This can be used when iterating over the geometry to ensure the sizes of
  /// the children are correctly set.
  int64_t size_nodes;
};

#endif

#ifdef __cplusplus
}
#endif
