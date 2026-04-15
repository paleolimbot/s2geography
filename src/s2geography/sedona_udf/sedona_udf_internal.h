#pragma once

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>

#include "geoarrow/geoarrow.hpp"
#include "nanoarrow/nanoarrow.hpp"
#include "s2geography.h"
#include "s2geography/geoarrow-geography.h"
#include "s2geography/sedona_udf/sedona_extension.h"

namespace s2geography {

namespace sedona_udf {

/// \brief Helper to detect unreachable code, for use in static_assert
template <class... T>
struct always_false : std::false_type {};

/// \brief Detection trait for optional Exec::Init(arg0_t*, out_t*) method
template <typename T, typename = void>
struct has_exec_init : std::false_type {};

template <typename T>
struct has_exec_init<T, std::void_t<decltype(std::declval<T>().Init(
                            std::declval<typename T::arg0_t*>(),
                            std::declval<typename T::out_t*>()))>>
    : std::true_type {};

/// \brief Detection trait for optional Exec::Init(arg0_t*, arg1_t*, out_t*)
/// method
template <typename T, typename = void>
struct has_exec_init_binary : std::false_type {};

template <typename T>
struct has_exec_init_binary<T, std::void_t<decltype(std::declval<T>().Init(
                                   std::declval<typename T::arg0_t*>(),
                                   std::declval<typename T::arg1_t*>(),
                                   std::declval<typename T::out_t*>()))>>
    : std::true_type {};

/// \brief Detection trait for optional Exec::Init(arg0_t*, arg1_t*, arg2_t*,
/// out_t*) method
template <typename T, typename = void>
struct has_exec_init_ternary : std::false_type {};

template <typename T>
struct has_exec_init_ternary<T, std::void_t<decltype(std::declval<T>().Init(
                                    std::declval<typename T::arg0_t*>(),
                                    std::declval<typename T::arg1_t*>(),
                                    std::declval<typename T::arg2_t*>(),
                                    std::declval<typename T::out_t*>()))>>
    : std::true_type {};

/// \defgroup sedona_udf-utils Arrow UDF Utilities
///
/// To simplify implementations of a large number of functions, we
/// define some templated abstractions to handle input and output.
/// Each argument gets its own input view and every scalar UDF
/// has one output builder.
///
/// Combinations that appear
/// - (geog) -> bool
/// - (geog) -> int
/// - (geog) -> double
/// - (geog) -> geog
/// - (geog, double) -> geog
/// - (geog, geog) -> bool
/// - (geog, geog) -> double
/// - (geog, geog, double) -> bool
/// - (geog, geog) -> geog
///
/// @{

/// \brief Calculate the number of iterations required for execution
///
/// Execute implementations are given an n_rows argument but also may have
/// been handed a collection of arguments that are entirely scalar (and thus
/// only require a single iteration even if n_rows is large).
inline int64_t ExecuteNumIterations(int64_t n_rows,
                                    struct ArrowArray* const* args,
                                    int64_t n_args) {
  if (n_args == 0) {
    return n_rows;
  }

  for (int64_t i = 0; i < n_args; i++) {
    // If any of the arguments have a size != 1, use the row count
    if (args[i]->length != 1) {
      return n_rows;
    }
  }

  // Otherwise, we have all scalar arguments
  return 1;
}

/// \brief Generic output builder for Arrow output
///
/// This output builder handles non-nested Arrow output using the
/// nanoarrow builder. This builder is not the fastest way to build this
/// output but it is relatively flexible. It may be faster to build
/// output include building a C++ vector and wrap that vector into
/// an array at the very end.
template <typename c_type_t, enum ArrowType arrow_type_val>
class ArrowOutputBuilder {
 public:
  using c_type = c_type_t;

  ArrowOutputBuilder() = default;
  ArrowOutputBuilder(const ArrowOutputBuilder&) = delete;
  ArrowOutputBuilder& operator=(const ArrowOutputBuilder&) = delete;

  void InitOutputType(struct ArrowSchema* out) {
    NANOARROW_THROW_NOT_OK(ArrowSchemaInitFromType(out, arrow_type_val));
  }

  void InitOutputTypeWithCrs(struct ArrowSchema* out, const std::string& crs) {
    S2GEOGRAPHY_UNUSED(crs);
    InitOutputType(out);
  }

  void Reserve(int64_t additional_size) {
    array_.reset();
    NANOARROW_THROW_NOT_OK(
        ArrowArrayInitFromType(array_.get(), arrow_type_val));
    NANOARROW_THROW_NOT_OK(ArrowArrayStartAppending(array_.get()));

    NANOARROW_THROW_NOT_OK(ArrowArrayReserve(array_.get(), additional_size));
  }

  void AppendNull() {
    NANOARROW_THROW_NOT_OK(ArrowArrayAppendNull(array_.get(), 1));
  }

  void AppendEmpty() {
    NANOARROW_THROW_NOT_OK(ArrowArrayAppendEmpty(array_.get(), 1));
  }

  void Append(c_type value) {
    if constexpr (std::is_integral_v<c_type>) {
      NANOARROW_THROW_NOT_OK(ArrowArrayAppendInt(array_.get(), value));
    } else if constexpr (std::is_floating_point_v<c_type>) {
      NANOARROW_THROW_NOT_OK(ArrowArrayAppendDouble(array_.get(), value));
    } else {
      static_assert(always_false<c_type>::value, "value type not supported");
    }
  }

  int64_t current_length() { return array_->length; }

  void Finish(struct ArrowArray* out) {
    NANOARROW_THROW_NOT_OK(
        ArrowArrayFinishBuildingDefault(array_.get(), nullptr));
    ArrowArrayMove(array_.get(), out);
  }

 protected:
  nanoarrow::UniqueArray array_;
};

using BoolOutputBuilder = ArrowOutputBuilder<bool, NANOARROW_TYPE_BOOL>;
using IntOutputBuilder = ArrowOutputBuilder<int64_t, NANOARROW_TYPE_INT64>;
using DoubleOutputBuilder = ArrowOutputBuilder<double, NANOARROW_TYPE_DOUBLE>;

template <typename Child>
class ListOutputBuilder {
 public:
  ListOutputBuilder() = default;
  ListOutputBuilder(const ListOutputBuilder&) = delete;
  ListOutputBuilder& operator=(const ListOutputBuilder&) = delete;

  Child& items() { return items_; }

  void InitOutputType(struct ArrowSchema* out) {
    ArrowSchemaInit(out);
    NANOARROW_THROW_NOT_OK(ArrowSchemaSetFormat(out, "+l"));
    NANOARROW_THROW_NOT_OK(ArrowSchemaAllocateChildren(out, 1));
    items_.InitOutputType(out->children[0]);
    NANOARROW_THROW_NOT_OK(ArrowSchemaSetName(out->children[0], "item"));
  }

  void InitOutputTypeWithCrs(struct ArrowSchema* out, const std::string& crs) {
    S2GEOGRAPHY_UNUSED(crs);
    InitOutputType(out);
  }

  void Reserve(int64_t additional_size) {
    array_.reset();

    current_length_ = 0;
    lengths_.clear();
    lengths_.reserve(additional_size + 1);
    lengths_.push_back(0);

    nulls_.clear();
    null_count_ = 0;

    // We could do a better job exposing the expected list size. It is
    // unlikely to have a list that is completely empty with all elements
    // and this could be optimized at some point.
    items_.Reserve(0);
  }

  void AppendNull() { Append(false); }

  void Append(bool is_valid = true) {
    if (items_.current_length() > std::numeric_limits<int32_t>::max()) {
      throw Exception(
          "Can't build nested list output with >INT32_MAX child elements");
    }

    if (nulls_.empty() && !is_valid) {
      nulls_.reserve(lengths_.capacity());
      nulls_.resize(current_length_);
      std::fill_n(nulls_.begin(), current_length_, 1);
      nulls_.push_back(0);
    } else if (!nulls_.empty()) {
      nulls_.push_back(is_valid);
    }

    lengths_.push_back(static_cast<int32_t>(items_.current_length()));

    null_count_ += !is_valid;
    ++current_length_;
  }

  void Finish(struct ArrowArray* out) {
    nanoarrow::UniqueArray tmp;
    NANOARROW_THROW_NOT_OK(
        ArrowArrayInitFromType(tmp.get(), NANOARROW_TYPE_LIST));
    NANOARROW_THROW_NOT_OK(ArrowArrayAllocateChildren(tmp.get(), 1));
    items_.Finish(tmp->children[0]);

    if (null_count_ > 0) {
      nanoarrow::UniqueBitmap nulls;
      ArrowBitmapInit(nulls.get());
      NANOARROW_THROW_NOT_OK(ArrowBitmapReserve(nulls.get(), current_length_));
      ArrowBitmapAppendInt8Unsafe(nulls.get(), nulls_.data(), current_length_);
      ArrowArraySetValidityBitmap(tmp.get(), nulls.get());
    }

    nanoarrow::UniqueBuffer offsets;
    nanoarrow::BufferInitSequence(offsets.get(), lengths_);
    lengths_.clear();
    NANOARROW_THROW_NOT_OK(ArrowArraySetBuffer(tmp.get(), 1, offsets.get()));

    // Set the array metadata
    tmp->length = current_length_;
    tmp->null_count = null_count_;

    NANOARROW_THROW_NOT_OK(ArrowArrayFinishBuildingDefault(tmp.get(), nullptr));
    ArrowArrayMove(tmp.get(), out);
  }

  Child items_;
  std::vector<int8_t> nulls_;
  std::vector<int32_t> lengths_;
  nanoarrow::UniqueArray array_;
  int64_t current_length_{};
  int64_t null_count_{};
};

template <typename... Children>
class StructOutputBuilder {
 public:
  StructOutputBuilder() = default;
  StructOutputBuilder(const StructOutputBuilder&) = delete;
  StructOutputBuilder& operator=(const StructOutputBuilder&) = delete;

  template <size_t I>
  auto& field() {
    return std::get<I>(fields_);
  }

  template <size_t I>
  const auto& field() const {
    return std::get<I>(fields_);
  }

  static constexpr size_t kNumFields = sizeof...(Children);

  void SetNames(const std::array<const char*, kNumFields>& names) {
    names_ = names;
  }

  void InitOutputType(struct ArrowSchema* out) {
    NANOARROW_THROW_NOT_OK(ArrowSchemaInitFromType(out, NANOARROW_TYPE_STRUCT));
    NANOARROW_THROW_NOT_OK(ArrowSchemaAllocateChildren(out, kNumFields));
    InitFieldsOutputType(out, std::index_sequence_for<Children...>{});
  }

  void InitOutputTypeWithCrs(struct ArrowSchema* out, const std::string& crs) {
    S2GEOGRAPHY_UNUSED(crs);
    InitOutputType(out);
  }

  void Reserve(int64_t additional_size) {
    current_length_ = 0;
    null_count_ = 0;
    nulls_.clear();
    ReserveFields(additional_size, std::index_sequence_for<Children...>{});
  }

  void AppendNull() {
    AppendEmptyFields(std::index_sequence_for<Children...>{});
    Append(false);
  }

  void Append(bool is_valid = true) {
    if (nulls_.empty() && !is_valid) {
      nulls_.reserve(current_length_ + 1);
      nulls_.resize(current_length_);
      std::fill_n(nulls_.begin(), current_length_, 1);
    }

    if (!nulls_.empty()) {
      nulls_.push_back(is_valid);
    }

    null_count_ += !is_valid;
    ++current_length_;
  }

  int64_t current_length() const { return current_length_; }

  void Finish(struct ArrowArray* out) {
    nanoarrow::UniqueArray tmp;
    NANOARROW_THROW_NOT_OK(
        ArrowArrayInitFromType(tmp.get(), NANOARROW_TYPE_STRUCT));
    NANOARROW_THROW_NOT_OK(
        ArrowArrayAllocateChildren(tmp.get(), sizeof...(Children)));
    FinishFields(tmp.get(), std::index_sequence_for<Children...>{});

    tmp->length = current_length_;
    tmp->null_count = null_count_;

    if (!nulls_.empty()) {
      nanoarrow::UniqueBitmap nulls;
      ArrowBitmapInit(nulls.get());
      NANOARROW_THROW_NOT_OK(ArrowBitmapReserve(nulls.get(), current_length_));
      ArrowBitmapAppendInt8Unsafe(nulls.get(), nulls_.data(), current_length_);
      ArrowArraySetValidityBitmap(tmp.get(), nulls.get());
    }

    ArrowArrayMove(tmp.get(), out);
  }

 private:
  std::tuple<Children...> fields_;
  std::vector<int8_t> nulls_;
  std::array<const char*, kNumFields> names_{};
  int64_t current_length_{0};
  int64_t null_count_{0};

  template <size_t... Is>
  void InitFieldsOutputType(struct ArrowSchema* out,
                            std::index_sequence<Is...>) {
    (InitField<Is>(out->children[Is]), ...);
  }

  template <size_t I>
  void InitField(struct ArrowSchema* child) {
    std::get<I>(fields_).InitOutputType(child);
    if (names_[I] != nullptr) {
      NANOARROW_THROW_NOT_OK(ArrowSchemaSetName(child, names_[I]));
    }
  }

  template <size_t... Is>
  void ReserveFields(int64_t additional_size, std::index_sequence<Is...>) {
    (std::get<Is>(fields_).Reserve(additional_size), ...);
  }

  template <size_t... Is>
  void AppendEmptyFields(std::index_sequence<Is...>) {
    (std::get<Is>(fields_).AppendEmpty(), ...);
  }

  template <size_t... Is>
  void FinishFields(struct ArrowArray* out, std::index_sequence<Is...>) {
    (std::get<Is>(fields_).Finish(out->children[Is]), ...);
  }
};

/// \brief Low-level output builder for Geography as WKB
///
/// This builder handles output from functions that return geometry
/// and exports the output as WKB. Unlike the WkbGeographyOutputBuilder,
/// this builder exposes low-level building primitives for faster output
/// (i.e., streaming output with minimal intermediary copying) and more
/// feature-rich (e.g., ZM output and lossless point/multipoint semantics).
class GeoArrowOutputBuilder {
 public:
  GeoArrowOutputBuilder() {
    // Initialize the writer and wire it up to the visitor
    GEOARROW_THROW_NOT_OK(nullptr, GeoArrowWKBWriterInit(&writer_));
    GeoArrowWKBWriterInitVisitor(&writer_, &v_);
    v_.error = &error_;

    // Wire up our coordinate buffer to a GeoArrowCoordView, which is what
    // the visitor requires for coord visiting.
    coords_.coords_stride = 1;
    coords_.n_values = 2;
    coords_.n_coords = 0;
    coords_.values[0] = coord_buf_.data();
    coords_.values[1] = coord_buf_.data() + coord_buf_.size() / 4;
    coords_.values[2] = coord_buf_.data() + 2 * coord_buf_.size() / 4;
    coords_.values[3] = coord_buf_.data() + 3 * coord_buf_.size() / 4;

    // Set the fill value for the unlikely event where the output is set to
    // write more dimensions than exist in a coordinate.
    coord_src_[0] = std::numeric_limits<double>::quiet_NaN();
  }

  // Not copyable
  GeoArrowOutputBuilder(const GeoArrowOutputBuilder&) = delete;
  GeoArrowOutputBuilder& operator=(const GeoArrowOutputBuilder&) = delete;

  // Ensure we manage the C object we're wrapping correctly
  ~GeoArrowOutputBuilder() { GeoArrowWKBWriterReset(&writer_); }

  void InitOutputType(struct ArrowSchema* out) {
    ::geoarrow::Wkb()
        .WithEdgeType(GEOARROW_EDGE_TYPE_SPHERICAL)
        .InitSchema(out);
  }

  void InitOutputTypeWithCrs(struct ArrowSchema* out, const std::string& crs) {
    ::geoarrow::Wkb()
        .WithEdgeType(GEOARROW_EDGE_TYPE_SPHERICAL)
        .WithCrs(crs)
        .InitSchema(out);
  }

  void Reserve(int64_t additional_size) {
    S2GEOGRAPHY_UNUSED(additional_size);
    // The current geoarrow writer doesn't provide any support for this;
    // however, it does support multiple cycles of Append/Finish.
  }

  /// \brief Set the dimensions of this writer to the common dimensions of dim0
  /// and dim1
  ///
  /// For example, XYZM + XYM input will be written set to XYM output.
  void SetDimensionsCommon(uint8_t dim0, uint8_t dim1) {
    if (dim0 == GEOARROW_DIMENSIONS_XY || dim1 == GEOARROW_DIMENSIONS_XY) {
      SetDimensions(GEOARROW_DIMENSIONS_XY);
    } else if (dim0 == GEOARROW_DIMENSIONS_XYZ &&
               dim1 == GEOARROW_DIMENSIONS_XYM) {
      SetDimensions(GEOARROW_DIMENSIONS_XY);
    } else if (dim0 == GEOARROW_DIMENSIONS_XYM &&
               dim1 == GEOARROW_DIMENSIONS_XYZ) {
      SetDimensions(GEOARROW_DIMENSIONS_XY);
    } else {
      SetDimensions(std::min(dim0, dim1));
    }
  }

  /// \brief Set the output dimensions
  ///
  /// Set the output to a specific dimensionality. Coordinates written with
  /// more dimensions will have these dimensions dropped; coordinates written
  /// with fewer dimensions will have these dimensions filled. The fill value
  /// is currently hard-coded to NaN; however, this output should be unlikely
  /// under normal input/output scenarios.
  void SetDimensions(uint8_t dim) {
    switch (dim) {
      case GEOARROW_DIMENSIONS_XY:
      case GEOARROW_DIMENSIONS_XYZ:
      case GEOARROW_DIMENSIONS_XYM:
      case GEOARROW_DIMENSIONS_XYZM:
        coords_.n_values = _GeoArrowkNumDimensions[dim];
        dim_ = static_cast<enum GeoArrowDimensions>(dim);
        break;
      default:
        throw Exception("Unknown dimensions constant");
    }
  }

  /// \brief Append a null value
  void AppendNull() {
    GEOARROW_THROW_NOT_OK(nullptr, GeoArrowWKBWriterAppendNull(&writer_));
  }

  /// \brief Append an empty geometry of a specified type
  void AppendEmpty(
      uint8_t geometry_type = GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION) {
    switch (geometry_type) {
      case GEOARROW_GEOMETRY_TYPE_POINT:
      case GEOARROW_GEOMETRY_TYPE_LINESTRING:
      case GEOARROW_GEOMETRY_TYPE_POLYGON:
      case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
      case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
      case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
      case GEOARROW_GEOMETRY_TYPE_GEOMETRYCOLLECTION:
        break;
      default:
        throw Exception("Unknown geometry type ID");
    }

    FeatureStart();
    GeomStart(static_cast<GeoArrowGeometryType>(geometry_type));
    GeomEnd();
    FeatureEnd();
  }

  void AppendPoint(const internal::GeoArrowVertex& pt,
                   uint8_t dim_src = GEOARROW_DIMENSIONS_XYZM) {
    FeatureStart();
    GeomStart(GEOARROW_GEOMETRY_TYPE_POINT);
    WriteCoord(pt, dim_src);
    GeomEnd();
    FeatureEnd();
  }

  /// \brief Append a preexisting geometry verbatim as a complete (non null)
  /// feature
  void AppendGeometry(struct GeoArrowGeometryView geom) {
    GEOARROW_THROW_NOT_OK(nullptr, GeoArrowWKBWriterAppend(&writer_, geom));
  }

  /// \brief Start a feature (must be paired with FeatureEnd())
  void FeatureStart() { GEOARROW_THROW_NOT_OK(&error_, v_.feat_start(&v_)); }

  /// \brief Start a geometry (must be paired with GeomEnd())
  void GeomStart(enum GeoArrowGeometryType geometry_type) {
    GEOARROW_THROW_NOT_OK(&error_, v_.geom_start(&v_, geometry_type, dim_));
  }

  /// \brief Start a ring (must be paired with RingEnd())
  void RingStart() { GEOARROW_THROW_NOT_OK(&error_, v_.ring_start(&v_)); }

  /// \brief Write an S2Point coordinate as lon, lat
  void WriteCoord(const S2Point& v) { WriteCoord(S2LatLng(v)); }

  /// \brief Write an S2LatLng coordinate as lon, lat
  void WriteCoord(const S2LatLng& v) {
    WriteCoord(v.lng().degrees(), v.lat().degrees());
  }

  /// \brief Write a GeoArrowVertex
  ///
  /// Dimensions are mapped using dim_src and the value set for the output
  /// (i.e., dimensions are mapped by name, not by position). It is usually
  /// easier to call v.Normalize() and use the default dim_src than to pass
  /// around the coordinate dimension separately.
  void WriteCoord(const internal::GeoArrowVertex& v,
                  uint8_t dim_src = GEOARROW_DIMENSIONS_XYZM) {
    if (coords_.n_coords == kCoordsCapacity) {
      FlushCoords();
    }

    if (dim_ == GEOARROW_DIMENSIONS_XY) {
      const_cast<double*>(coords_.values[0])[coords_.n_coords] = v.lng;
      const_cast<double*>(coords_.values[1])[coords_.n_coords] = v.lat;
      ++coords_.n_coords;
      return;
    }

    int map[5];
    GeoArrowMapDimensions(static_cast<enum GeoArrowDimensions>(dim_src), dim_,
                          map);
    std::memcpy(coord_src_ + 1, &v, sizeof(v));
    for (int i = 0; i < 4; i++) {
      coord_dst_[i] = coord_src_[map[i] + 1];
    }

    for (int i = 0; i < coords_.n_values; ++i) {
      const_cast<double*>(coords_.values[i])[coords_.n_coords] = coord_dst_[i];
    }
    ++coords_.n_coords;
  }

  /// \brief End a ring
  void RingEnd() {
    FlushCoords();
    GEOARROW_THROW_NOT_OK(&error_, v_.ring_end(&v_));
  }

  /// \brief End a geometry
  void GeomEnd() {
    FlushCoords();
    GEOARROW_THROW_NOT_OK(&error_, v_.geom_end(&v_));
  }

  /// \brief End a feature
  void FeatureEnd() { GEOARROW_THROW_NOT_OK(&error_, v_.feat_end(&v_)); }

  /// \brief Finish the output
  ///
  /// The same output builder may be finished and appended to multiple times.
  void Finish(struct ArrowArray* out) {
    GEOARROW_THROW_NOT_OK(&error_,
                          GeoArrowWKBWriterFinish(&writer_, out, &error_));
  }

 private:
  GeoArrowWKBWriter writer_{};
  GeoArrowVisitor v_{};
  GeoArrowError error_{};
  enum GeoArrowDimensions dim_ { GEOARROW_DIMENSIONS_XY };
  struct GeoArrowCoordView coords_{};
  std::array<double, 64> coord_buf_{};
  static constexpr int64_t kCoordsCapacity = 64 / 4;
  double coord_src_[5];
  double coord_dst_[5];

  void WriteCoord(double x, double y) {
    if (coords_.n_coords == kCoordsCapacity) {
      FlushCoords();
    }

    const_cast<double*>(coords_.values[0])[coords_.n_coords] = x;
    const_cast<double*>(coords_.values[1])[coords_.n_coords] = y;
    for (int i = 2; i < coords_.n_values; ++i) {
      const_cast<double*>(coords_.values[i])[coords_.n_coords] = coord_src_[0];
    }

    ++coords_.n_coords;
  }

  void FlushCoords() {
    if (coords_.n_coords == 0) {
      return;
    }

    GEOARROW_THROW_NOT_OK(&error_, v_.coords(&v_, &coords_));
    coords_.n_coords = 0;
  }
};

/// \brief Generic view of Arrow input
///
/// This input viewer uses nanoarrow's ArrowArrayView to provide
/// random access to array elements. This is not the fastest way to
/// do this but does nicely handle multiple input types (e.g., any
/// integral type when accepting an integer as an argument) and nulls.
/// The functions we expose here are probably limited by the speed at
/// which the geometry can be decoded rather than iteration over
/// primitive arrays; however, we could attempt optimizing this if
/// we expose cheaper functions in this way.
template <typename c_type_t>
class ArrowInputView {
 public:
  using c_type = c_type_t;

  static bool Matches(const struct ArrowSchema* type) {
    struct ArrowSchemaView schema_view;
    NANOARROW_THROW_NOT_OK(ArrowSchemaViewInit(&schema_view, type, nullptr));

    if (schema_view.extension_name.data != nullptr) {
      return false;
    }

    if constexpr (std::is_same_v<c_type_t, bool>) {
      switch (schema_view.type) {
        case NANOARROW_TYPE_BOOL:
          return true;

        default:
          return false;
      }
    } else if constexpr (std::is_same_v<c_type_t, int64_t>) {
      switch (schema_view.type) {
        case NANOARROW_TYPE_INT8:
        case NANOARROW_TYPE_UINT8:
        case NANOARROW_TYPE_INT16:
        case NANOARROW_TYPE_UINT16:
        case NANOARROW_TYPE_INT32:
        case NANOARROW_TYPE_UINT32:
        case NANOARROW_TYPE_INT64:
        case NANOARROW_TYPE_UINT64:
          return true;

        default:
          return false;
      }
    } else if constexpr (std::is_same_v<c_type_t, double>) {
      switch (schema_view.type) {
        case NANOARROW_TYPE_INT8:
        case NANOARROW_TYPE_UINT8:
        case NANOARROW_TYPE_INT16:
        case NANOARROW_TYPE_UINT16:
        case NANOARROW_TYPE_INT32:
        case NANOARROW_TYPE_UINT32:
        case NANOARROW_TYPE_INT64:
        case NANOARROW_TYPE_UINT64:
        case NANOARROW_TYPE_HALF_FLOAT:
        case NANOARROW_TYPE_FLOAT:
        case NANOARROW_TYPE_DOUBLE:
          return true;

        default:
          return false;
      }
    } else if constexpr (std::is_same_v<c_type_t, std::string_view>) {
      switch (schema_view.type) {
        case NANOARROW_TYPE_STRING:
        case NANOARROW_TYPE_STRING_VIEW:
        case NANOARROW_TYPE_LARGE_STRING:
        case NANOARROW_TYPE_BINARY:
        case NANOARROW_TYPE_BINARY_VIEW:
        case NANOARROW_TYPE_LARGE_BINARY:
          return true;

        default:
          return false;
      }
    } else {
      static_assert(always_false<c_type>::value, "value type not supported");
    }
  }

  static std::string GetCrs(const struct ArrowSchema* type) {
    S2GEOGRAPHY_UNUSED(type);
    return "";
  }

  ArrowInputView(const struct ArrowSchema* type) {
    NANOARROW_THROW_NOT_OK(
        ArrowArrayViewInitFromSchema(view_.get(), type, nullptr));
  }
  ArrowInputView(const ArrowInputView&) = delete;
  ArrowInputView& operator=(const ArrowInputView&) = delete;

  void SetPrepareScalar(bool prepare_scalar) {
    S2GEOGRAPHY_UNUSED(prepare_scalar);
  }

  void SetArray(const struct ArrowArray* array, int64_t num_rows) {
    S2GEOGRAPHY_UNUSED(num_rows);
    NANOARROW_THROW_NOT_OK(ArrowArrayViewSetArray(view_.get(), array, nullptr));

    if (array->length == 0) {
      throw Exception("Array input must not be empty");
    }
  }

  bool IsNull(int64_t i) {
    return ArrowArrayViewIsNull(view_.get(), i % view_->length);
  }

  c_type_t Get(int64_t i) {
    if constexpr (std::is_same_v<c_type_t, bool>) {
      return ArrowArrayViewGetIntUnsafe(view_.get(), i % view_->length);
    } else if constexpr (std::is_same_v<c_type_t, int64_t>) {
      return ArrowArrayViewGetIntUnsafe(view_.get(), i % view_->length);
    } else if constexpr (std::is_same_v<c_type, double>) {
      return ArrowArrayViewGetDoubleUnsafe(view_.get(), i % view_->length);
    } else if constexpr (std::is_same_v<c_type_t, std::string_view>) {
      struct ArrowBufferView val =
          ArrowArrayViewGetBytesUnsafe(view_.get(), i % view_->length);
      return std::string_view(val.data.as_char,
                              static_cast<size_t>(val.size_bytes));
    } else {
      static_assert(always_false<c_type>::value, "value type not supported");
    }
  }

 private:
  nanoarrow::UniqueArrayView view_;
};

using BoolInputView = ArrowInputView<bool>;
using IntInputView = ArrowInputView<int64_t>;
using DoubleInputView = ArrowInputView<double>;
using StringInputView = ArrowInputView<std::string_view>;

/// \brief View of GeoArrow input
///
/// This currently handles geoarrow.wkb arrays, although in theory can
/// represent any GeoArrow type when supported by geoarrow-c.
class GeoArrowGeographyInputView {
 public:
  using c_type = GeoArrowGeography&;

  static bool Matches(const struct ArrowSchema* type) {
    struct GeoArrowSchemaView schema_view;
    int err_code = GeoArrowSchemaViewInit(&schema_view, type, nullptr);
    if (err_code != GEOARROW_OK) {
      return false;
    }

    // Only handles WKB for now
    switch (schema_view.type) {
      case GEOARROW_TYPE_WKB:
      case GEOARROW_TYPE_WKB_VIEW:
      case GEOARROW_TYPE_LARGE_WKB:
        break;
      default:
        return false;
    }

    struct GeoArrowMetadataView metadata_view;
    err_code = GeoArrowMetadataViewInit(
        &metadata_view, schema_view.extension_metadata, nullptr);
    return err_code == GEOARROW_OK &&
           metadata_view.edge_type == GEOARROW_EDGE_TYPE_SPHERICAL;
  }

  GeoArrowGeographyInputView(const struct ArrowSchema* type)
      : inner_(type), current_array_length_(1), stashed_index_(-1) {
    type_ = ::geoarrow::GeometryDataType::Make(type);
    GEOARROW_THROW_NOT_OK(nullptr, GeoArrowWKBReaderInit(&reader_));
  }
  GeoArrowGeographyInputView(const GeoArrowGeographyInputView&) = delete;
  GeoArrowGeographyInputView& operator=(const GeoArrowGeographyInputView&) =
      delete;

  ~GeoArrowGeographyInputView() { GeoArrowWKBReaderReset(&reader_); }

  std::string GetCrs() { return type_.crs(); }

  void SetPrepareScalar(bool prepare_scalar) {
    prepare_scalar_ = prepare_scalar;
  }

  void SetArray(const struct ArrowArray* array, int64_t num_rows) {
    inner_.SetArray(array, num_rows);
    current_array_length_ = array->length;
    stashed_index_ = -1;
  }

  bool IsNull(int64_t i) { return inner_.IsNull(i); }

  GeoArrowGeography& Get(int64_t i) {
    if (current_array_length_ == 1) {
      StashIfNeeded(0, prepare_scalar_);
    } else {
      StashIfNeeded(i);
    }

    return stashed_;
  }

 private:
  ::geoarrow::GeometryDataType type_;
  struct GeoArrowWKBReader reader_;
  ArrowInputView<std::string_view> inner_;
  int64_t current_array_length_;
  int64_t stashed_index_;
  GeoArrowGeography stashed_;
  bool prepare_scalar_{};

  void StashIfNeeded(int64_t i, bool prepare = false) {
    if (i != stashed_index_) {
      std::string_view inner = inner_.Get(i);
      struct GeoArrowBufferView src = {
          reinterpret_cast<const uint8_t*>(inner.data()),
          static_cast<int64_t>(inner.size())};

      struct GeoArrowGeometryView geom{};
      GEOARROW_THROW_NOT_OK(
          nullptr, GeoArrowWKBReaderRead(&reader_, src, &geom, nullptr));

      stashed_.Init(geom);
      stashed_index_ = i;

      if (prepare) {
        stashed_.ForceBuildIndex();
      }
    }
  }
};

/// @}

/// \defgroup sedona-kernel-adapters Sedona C Scalar Kernel Adapters
///
/// These adapters wrap the Exec-based UDF pattern into the
/// SedonaCScalarKernel / SedonaCScalarKernelImpl C ABI defined in
/// sedona_extension.h.
///
/// @{

/// \brief Private data for SedonaCScalarKernel
struct KernelData {
  std::string name;
  bool prepare_arg0_scalar{true};
  bool prepare_arg1_scalar{true};
  bool prepare_arg2_scalar{true};
};

inline const char* KernelFunctionName(const struct SedonaCScalarKernel* self) {
  return static_cast<KernelData*>(self->private_data)->name.c_str();
}

inline void KernelRelease(struct SedonaCScalarKernel* self) {
  if (self->private_data != nullptr) {
    delete static_cast<KernelData*>(self->private_data);
    self->private_data = nullptr;
  }
  self->release = nullptr;
}

/// \brief Sedona C ABI adapter for unary UDFs (one argument)
template <typename Exec>
class SedonaUnaryKernelAdapter {
 public:
  struct ImplData {
    std::string last_error;
    std::unique_ptr<typename Exec::arg0_t> arg0;
    std::unique_ptr<typename Exec::out_t> out;
    Exec exec;
    bool prepare_arg0_scalar{true};
  };

  static int ImplInit(struct SedonaCScalarKernelImpl* self,
                      const struct ArrowSchema* const* arg_types,
                      struct ArrowArray* const* /*scalar_args*/, int64_t n_args,
                      struct ArrowSchema* out) {
    auto* data = static_cast<ImplData*>(self->private_data);
    data->last_error.clear();
    try {
      // Check if this kernel applies to the input arguments
      if (n_args != 1 || !Exec::arg0_t::Matches(arg_types[0])) {
        out->release = nullptr;
        return NANOARROW_OK;
      }

      data->arg0 = std::make_unique<typename Exec::arg0_t>(arg_types[0]);
      data->arg0->SetPrepareScalar(data->prepare_arg0_scalar);
      data->out = std::make_unique<typename Exec::out_t>();

      if constexpr (has_exec_init<Exec>::value) {
        data->exec.Init(data->arg0.get(), data->out.get());
      }

      std::string crs_out = data->arg0->GetCrs();
      if (crs_out.empty()) {
        data->out->InitOutputType(out);
      } else {
        data->out->InitOutputTypeWithCrs(out, crs_out);
      }

      return NANOARROW_OK;
    } catch (std::exception& e) {
      data->last_error = e.what();
      return EINVAL;
    }
  }

  static int ImplExecute(struct SedonaCScalarKernelImpl* self,
                         struct ArrowArray* const* args, int64_t n_args,
                         int64_t n_rows, struct ArrowArray* out) {
    auto* data = static_cast<ImplData*>(self->private_data);
    data->last_error.clear();
    try {
      if (n_args != 1) {
        data->last_error = "Expected one argument in unary s2geography kernel";
        return EINVAL;
      }

      data->arg0->SetArray(args[0], n_rows);
      int64_t num_iterations = ExecuteNumIterations(n_rows, args, n_args);
      data->out->Reserve(num_iterations);

      for (int64_t i = 0; i < num_iterations; i++) {
        if (data->arg0->IsNull(i)) {
          data->out->AppendNull();
        } else {
          typename Exec::arg0_t::c_type item0 = data->arg0->Get(i);
          data->exec.Exec(item0, data->out.get());
        }
      }

      data->out->Finish(out);
      return NANOARROW_OK;
    } catch (std::exception& e) {
      data->last_error = e.what();
      return EINVAL;
    }
  }

  static const char* ImplGetLastError(struct SedonaCScalarKernelImpl* self) {
    return static_cast<ImplData*>(self->private_data)->last_error.c_str();
  }

  static void ImplRelease(struct SedonaCScalarKernelImpl* self) {
    if (self->private_data != nullptr) {
      delete static_cast<ImplData*>(self->private_data);
      self->private_data = nullptr;
    }
    self->release = nullptr;
  }

  static void NewImpl(const struct SedonaCScalarKernel* self,
                      struct SedonaCScalarKernelImpl* out) {
    auto* kernel_private = static_cast<KernelData*>(self->private_data);
    auto* impl_private = new ImplData();
    impl_private->prepare_arg0_scalar = kernel_private->prepare_arg0_scalar;

    out->private_data = impl_private;
    out->init = &ImplInit;
    out->execute = &ImplExecute;
    out->get_last_error = &ImplGetLastError;
    out->release = &ImplRelease;
  }
};

/// \brief Sedona C ABI adapter for binary UDFs (two arguments)
template <typename Exec>
class SedonaBinaryKernelAdapter {
 public:
  struct ImplData {
    std::string last_error;
    std::unique_ptr<typename Exec::arg0_t> arg0;
    std::unique_ptr<typename Exec::arg1_t> arg1;
    std::unique_ptr<typename Exec::out_t> out;
    Exec exec;
    bool prepare_arg0_scalar{true};
    bool prepare_arg1_scalar{true};
  };

  static int ImplInit(struct SedonaCScalarKernelImpl* self,
                      const struct ArrowSchema* const* arg_types,
                      struct ArrowArray* const* /*scalar_args*/, int64_t n_args,
                      struct ArrowSchema* out) {
    auto* data = static_cast<ImplData*>(self->private_data);
    data->last_error.clear();
    try {
      // Check if this kernel applies to the input arguments
      if (n_args != 2 || !Exec::arg0_t::Matches(arg_types[0]) ||
          !Exec::arg1_t::Matches(arg_types[1])) {
        out->release = nullptr;
        return NANOARROW_OK;
      }

      data->arg0 = std::make_unique<typename Exec::arg0_t>(arg_types[0]);
      data->arg1 = std::make_unique<typename Exec::arg1_t>(arg_types[1]);
      data->arg0->SetPrepareScalar(data->prepare_arg0_scalar);
      data->arg1->SetPrepareScalar(data->prepare_arg1_scalar);
      data->out = std::make_unique<typename Exec::out_t>();

      if constexpr (has_exec_init_binary<Exec>::value) {
        data->exec.Init(data->arg0.get(), data->arg1.get(), data->out.get());
      }

      // We don't have a reliable way to check the equality of CRSes, so
      // here we just return the first CRS.
      std::string crs_out = data->arg0->GetCrs();
      if (crs_out.empty()) {
        data->out->InitOutputType(out);
      } else {
        data->out->InitOutputTypeWithCrs(out, crs_out);
      }

      return 0;
    } catch (std::exception& e) {
      data->last_error = e.what();
      return EINVAL;
    }
  }

  static int ImplExecute(struct SedonaCScalarKernelImpl* self,
                         struct ArrowArray* const* args, int64_t n_args,
                         int64_t n_rows, struct ArrowArray* out) {
    auto* data = static_cast<ImplData*>(self->private_data);
    data->last_error.clear();
    try {
      if (n_args != 2) {
        data->last_error =
            "Expected two arguments in binary s2geography kernel";
        return EINVAL;
      }

      data->arg0->SetArray(args[0], n_rows);
      data->arg1->SetArray(args[1], n_rows);
      int64_t num_iterations = ExecuteNumIterations(n_rows, args, n_args);
      data->out->Reserve(num_iterations);

      for (int64_t i = 0; i < num_iterations; i++) {
        if (data->arg0->IsNull(i) || data->arg1->IsNull(i)) {
          data->out->AppendNull();
        } else {
          typename Exec::arg0_t::c_type item0 = data->arg0->Get(i);
          typename Exec::arg1_t::c_type item1 = data->arg1->Get(i);
          data->exec.Exec(item0, item1, data->out.get());
        }
      }

      data->out->Finish(out);
      return 0;
    } catch (std::exception& e) {
      data->last_error = e.what();
      return EINVAL;
    }
  }

  static const char* ImplGetLastError(struct SedonaCScalarKernelImpl* self) {
    return static_cast<ImplData*>(self->private_data)->last_error.c_str();
  }

  static void ImplRelease(struct SedonaCScalarKernelImpl* self) {
    if (self->private_data != nullptr) {
      delete static_cast<ImplData*>(self->private_data);
      self->private_data = nullptr;
    }
    self->release = nullptr;
  }

  static void NewImpl(const struct SedonaCScalarKernel* self,
                      struct SedonaCScalarKernelImpl* out) {
    auto* kernel_private = static_cast<KernelData*>(self->private_data);
    auto* impl_private = new ImplData();
    impl_private->prepare_arg0_scalar = kernel_private->prepare_arg0_scalar;
    impl_private->prepare_arg1_scalar = kernel_private->prepare_arg1_scalar;

    out->private_data = impl_private;
    out->init = &ImplInit;
    out->execute = &ImplExecute;
    out->get_last_error = &ImplGetLastError;
    out->release = &ImplRelease;
  }
};

/// \brief Sedona C ABI adapter for ternary UDFs (three arguments)
template <typename Exec>
class SedonaTernaryKernelAdapter {
 public:
  struct ImplData {
    std::string last_error;
    std::unique_ptr<typename Exec::arg0_t> arg0;
    std::unique_ptr<typename Exec::arg1_t> arg1;
    std::unique_ptr<typename Exec::arg2_t> arg2;
    std::unique_ptr<typename Exec::out_t> out;
    Exec exec;
    bool prepare_arg0_scalar{true};
    bool prepare_arg1_scalar{true};
    bool prepare_arg2_scalar{true};
  };

  static int ImplInit(struct SedonaCScalarKernelImpl* self,
                      const struct ArrowSchema* const* arg_types,
                      struct ArrowArray* const* /*scalar_args*/, int64_t n_args,
                      struct ArrowSchema* out) {
    auto* data = static_cast<ImplData*>(self->private_data);
    data->last_error.clear();
    try {
      // Check if this kernel applies to the input arguments
      if (n_args != 3 || !Exec::arg0_t::Matches(arg_types[0]) ||
          !Exec::arg1_t::Matches(arg_types[1]) ||
          !Exec::arg2_t::Matches(arg_types[2])) {
        out->release = nullptr;
        return NANOARROW_OK;
      }

      data->arg0 = std::make_unique<typename Exec::arg0_t>(arg_types[0]);
      data->arg1 = std::make_unique<typename Exec::arg1_t>(arg_types[1]);
      data->arg2 = std::make_unique<typename Exec::arg2_t>(arg_types[2]);
      data->arg0->SetPrepareScalar(data->prepare_arg0_scalar);
      data->arg1->SetPrepareScalar(data->prepare_arg1_scalar);
      data->arg2->SetPrepareScalar(data->prepare_arg2_scalar);
      data->out = std::make_unique<typename Exec::out_t>();

      if constexpr (has_exec_init_ternary<Exec>::value) {
        data->exec.Init(data->arg0.get(), data->arg1.get(), data->arg2.get(),
                        data->out.get());
      }

      // We don't have a reliable way to check the equality of CRSes, so
      // here we just return the first CRS.
      std::string crs_out = data->arg0->GetCrs();
      if (crs_out.empty()) {
        data->out->InitOutputType(out);
      } else {
        data->out->InitOutputTypeWithCrs(out, crs_out);
      }

      return 0;
    } catch (std::exception& e) {
      data->last_error = e.what();
      return EINVAL;
    }
  }

  static int ImplExecute(struct SedonaCScalarKernelImpl* self,
                         struct ArrowArray* const* args, int64_t n_args,
                         int64_t n_rows, struct ArrowArray* out) {
    auto* data = static_cast<ImplData*>(self->private_data);
    data->last_error.clear();
    try {
      if (n_args != 3) {
        data->last_error =
            "Expected three arguments in ternary s2geography kernel";
        return EINVAL;
      }

      data->arg0->SetArray(args[0], n_rows);
      data->arg1->SetArray(args[1], n_rows);
      data->arg2->SetArray(args[2], n_rows);
      int64_t num_iterations = ExecuteNumIterations(n_rows, args, n_args);
      data->out->Reserve(num_iterations);

      for (int64_t i = 0; i < num_iterations; i++) {
        if (data->arg0->IsNull(i) || data->arg1->IsNull(i) ||
            data->arg2->IsNull(i)) {
          data->out->AppendNull();
        } else {
          typename Exec::arg0_t::c_type item0 = data->arg0->Get(i);
          typename Exec::arg1_t::c_type item1 = data->arg1->Get(i);
          typename Exec::arg2_t::c_type item2 = data->arg2->Get(i);
          data->exec.Exec(item0, item1, item2, data->out.get());
        }
      }

      data->out->Finish(out);
      return 0;
    } catch (std::exception& e) {
      data->last_error = e.what();
      return EINVAL;
    }
  }

  static const char* ImplGetLastError(struct SedonaCScalarKernelImpl* self) {
    return static_cast<ImplData*>(self->private_data)->last_error.c_str();
  }

  static void ImplRelease(struct SedonaCScalarKernelImpl* self) {
    if (self->private_data != nullptr) {
      delete static_cast<ImplData*>(self->private_data);
      self->private_data = nullptr;
    }
    self->release = nullptr;
  }

  static void NewImpl(const struct SedonaCScalarKernel* self,
                      struct SedonaCScalarKernelImpl* out) {
    auto* kernel_private = static_cast<KernelData*>(self->private_data);
    auto* impl_private = new ImplData();
    impl_private->prepare_arg0_scalar = kernel_private->prepare_arg0_scalar;
    impl_private->prepare_arg1_scalar = kernel_private->prepare_arg1_scalar;
    impl_private->prepare_arg2_scalar = kernel_private->prepare_arg2_scalar;

    out->private_data = impl_private;
    out->init = &ImplInit;
    out->execute = &ImplExecute;
    out->get_last_error = &ImplGetLastError;
    out->release = &ImplRelease;
  }
};

/// \brief Initialize a SedonaCScalarKernel for a unary Exec
template <typename Exec>
void InitUnaryKernel(struct SedonaCScalarKernel* out, const char* name,
                     bool prepare_arg0_scalar = true) {
  auto* data = new KernelData{name, prepare_arg0_scalar};
  out->private_data = data;
  out->function_name = &KernelFunctionName;
  out->new_impl = &SedonaUnaryKernelAdapter<Exec>::NewImpl;
  out->release = &KernelRelease;
}

/// \brief Initialize a SedonaCScalarKernel for a binary Exec
template <typename Exec>
void InitBinaryKernel(struct SedonaCScalarKernel* out, const char* name,
                      bool prepare_arg0_scalar = true,
                      bool prepare_arg1_scalar = true) {
  auto* data = new KernelData{name, prepare_arg0_scalar, prepare_arg1_scalar};
  out->private_data = data;
  out->function_name = &KernelFunctionName;
  out->new_impl = &SedonaBinaryKernelAdapter<Exec>::NewImpl;
  out->release = &KernelRelease;
}

/// \brief Initialize a SedonaCScalarKernel for a ternary Exec
template <typename Exec>
void InitTernaryKernel(struct SedonaCScalarKernel* out, const char* name,
                       bool prepare_arg0_scalar = true,
                       bool prepare_arg1_scalar = true,
                       bool prepare_arg2_scalar = true) {
  auto* data = new KernelData{name, prepare_arg0_scalar, prepare_arg1_scalar,
                              prepare_arg2_scalar};
  out->private_data = data;
  out->function_name = &KernelFunctionName;
  out->new_impl = &SedonaTernaryKernelAdapter<Exec>::NewImpl;
  out->release = &KernelRelease;
}

/// @}

}  // namespace sedona_udf

}  // namespace s2geography
