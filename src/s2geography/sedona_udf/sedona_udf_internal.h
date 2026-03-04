#pragma once

#include <cerrno>

#include "geoarrow/geoarrow.hpp"
#include "nanoarrow/nanoarrow.hpp"
#include "s2geography.h"
#include "s2geography/sedona_udf/sedona_extension.h"

namespace s2geography {

namespace sedona_udf {

/// \brief Helper to detect unreachable code, for use in static_assert
template <class... T>
struct always_false : std::false_type {};

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

  ArrowOutputBuilder() {
    NANOARROW_THROW_NOT_OK(
        ArrowArrayInitFromType(array_.get(), arrow_type_val));
    NANOARROW_THROW_NOT_OK(ArrowArrayStartAppending(array_.get()));
  }

  void InitOutputType(struct ArrowSchema* out) {
    NANOARROW_THROW_NOT_OK(ArrowSchemaInitFromType(out, arrow_type_val));
  }

  void InitOutputTypeWithCrs(struct ArrowSchema* out, const std::string& crs) {
    InitOutputType(out);
  }

  void Reserve(int64_t additional_size) {
    NANOARROW_THROW_NOT_OK(ArrowArrayReserve(array_.get(), additional_size));
  }

  void AppendNull() {
    NANOARROW_THROW_NOT_OK(ArrowArrayAppendNull(array_.get(), 1));
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

  void Finish(struct ArrowArray* out) {
    NANOARROW_THROW_NOT_OK(
        ArrowArrayFinishBuildingDefault(array_.get(), nullptr));
    ArrowArrayMove(array_.get(), out);
  }

 protected:
  nanoarrow::UniqueArray array_;
};

using BoolOutputBuilder = ArrowOutputBuilder<bool, NANOARROW_TYPE_BOOL>;
using IntOutputBuilder = ArrowOutputBuilder<int32_t, NANOARROW_TYPE_INT32>;
using DoubleOutputBuilder = ArrowOutputBuilder<double, NANOARROW_TYPE_DOUBLE>;

/// \brief Output builder for Geography as WKB
///
/// This builder handles output from functions that return geometry
/// and exports the output as WKB. This is probably slow in many cases
/// and could possibly be accelerated by returning the "encoded" form
/// or by circumventing the GeoArrow writer entirely to build point output
/// (other than the boolean operation, functions that return geographies
/// mostly return points or line segments).
class WkbGeographyOutputBuilder {
 public:
  using c_type = const Geography&;

  WkbGeographyOutputBuilder() {
    writer_.Init(geoarrow::Writer::OutputType::kWKB, geoarrow::ExportOptions());
  }

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
    // The current geoarrow writer doesn't provide any support for this
  }

  void AppendNull() { writer_.WriteNull(); }

  void Append(c_type value) { writer_.WriteGeography(value); }

  void Finish(struct ArrowArray* out) { writer_.Finish(out); }

 private:
  geoarrow::Writer writer_;
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
    } else {
      static_assert(always_false<c_type>::value, "value type not supported");
    }
  }

  static std::string GetCrs(const struct ArrowSchema* type) { return ""; }

  ArrowInputView(const struct ArrowSchema* type) {
    NANOARROW_THROW_NOT_OK(
        ArrowArrayViewInitFromSchema(view_.get(), type, nullptr));
  }

  void SetArray(const struct ArrowArray* array, int64_t num_rows) {
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

/// \brief View of geography input
///
/// This handles any GeoArrow array as input. The return type is a reference
/// because the decoding is stashed for each element. This is essential for
/// the scalar case, where a single element would otherwise be decoded
/// thousands of time. This decoding is a particularly slow feature of
/// s2geography and is probably the first place to look to accelerate...either
/// by avoiding an abstract Geometry completely or by using the encoded form
/// instead of WKB to avoid the simple features--s2 conversion overhead.
class GeographyInputView {
 public:
  using c_type = const Geography&;

  static bool Matches(const struct ArrowSchema* type) {
    struct GeoArrowSchemaView schema_view;
    int err_code = GeoArrowSchemaViewInit(&schema_view, type, nullptr);
    if (err_code != GEOARROW_OK) {
      return false;
    }

    struct GeoArrowMetadataView metadata_view;
    err_code = GeoArrowMetadataViewInit(
        &metadata_view, schema_view.extension_metadata, nullptr);
    return err_code == GEOARROW_OK &&
           metadata_view.edge_type == GEOARROW_EDGE_TYPE_SPHERICAL;
  }

  GeographyInputView(const struct ArrowSchema* type)
      : current_array_(nullptr), stashed_index_(-1) {
    type_ = ::geoarrow::GeometryDataType::Make(type);
    reader_.Init(type);
  }

  std::string GetCrs() { return type_.crs(); }

  void SetArray(const struct ArrowArray* array, int64_t num_rows) {
    current_array_ = array;
    stashed_index_ = -1;
  }

  bool IsNull(int64_t i) {
    StashIfNeeded(i % current_array_->length);
    return stashed_[0].get() == nullptr;
  }

  const Geography& Get(int64_t i) {
    StashIfNeeded(i % current_array_->length);
    return *stashed_[0];
  }

 private:
  ::geoarrow::GeometryDataType type_;
  geoarrow::Reader reader_;
  const struct ArrowArray* current_array_;
  int64_t stashed_index_;
  std::vector<std::unique_ptr<Geography>> stashed_;

  void StashIfNeeded(int64_t i) {
    if (i != stashed_index_) {
      stashed_.clear();
      reader_.ReadGeography(current_array_, i, 1, &stashed_);
      stashed_index_ = i;
    }
  }
};

/// \brief View of geometry index input
///
/// This is used for operations like the S2BooleanOperation that require
/// a ShapeIndexGeometry as input. Like the GeographyInputView, we stash
/// the decoded value to avoid decoding and indexing a scalar input more
/// than once.
class GeographyIndexInputView {
 public:
  using c_type = const ShapeIndexGeography&;

  static bool Matches(const struct ArrowSchema* type) {
    return GeographyInputView::Matches(type);
  }

  GeographyIndexInputView(const struct ArrowSchema* type)
      : inner_(type), stashed_index_(-1) {}

  std::string GetCrs() { return inner_.GetCrs(); }

  void SetArray(const struct ArrowArray* array, int64_t num_rows) {
    stashed_index_ = -1;
    inner_.SetArray(array, num_rows);
    current_array_length_ = array->length;
  }

  bool IsNull(int64_t i) { return inner_.IsNull(i); }

  const ShapeIndexGeography& Get(int64_t i) {
    StashIfNeeded(i % current_array_length_);
    return stashed_;
  }

 private:
  GeographyInputView inner_;
  int64_t current_array_length_;
  int64_t stashed_index_;
  ShapeIndexGeography stashed_;

  void StashIfNeeded(int64_t i) {
    if (i != stashed_index_) {
      const auto& geog = inner_.Get(i);
      stashed_ = ShapeIndexGeography(geog);
      stashed_index_ = i;
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
      data->out = std::make_unique<typename Exec::out_t>();
      data->exec.Init({});

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
          typename Exec::out_t::c_type item_out = data->exec.Exec(item0);
          data->out->Append(item_out);
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

  static void NewImpl(const struct SedonaCScalarKernel* /*kernel*/,
                      struct SedonaCScalarKernelImpl* out) {
    out->private_data = new ImplData();
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
      data->out = std::make_unique<typename Exec::out_t>();
      data->exec.Init({});

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
          typename Exec::out_t::c_type item_out = data->exec.Exec(item0, item1);
          data->out->Append(item_out);
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

  static void NewImpl(const struct SedonaCScalarKernel* /*kernel*/,
                      struct SedonaCScalarKernelImpl* out) {
    out->private_data = new ImplData();
    out->init = &ImplInit;
    out->execute = &ImplExecute;
    out->get_last_error = &ImplGetLastError;
    out->release = &ImplRelease;
  }
};

/// \brief Initialize a SedonaCScalarKernel for a unary Exec
template <typename Exec>
void InitUnaryKernel(struct SedonaCScalarKernel* out, const char* name) {
  auto* data = new KernelData();
  data->name = name;
  out->private_data = data;
  out->function_name = &KernelFunctionName;
  out->new_impl = &SedonaUnaryKernelAdapter<Exec>::NewImpl;
  out->release = &KernelRelease;
}

/// \brief Initialize a SedonaCScalarKernel for a binary Exec
template <typename Exec>
void InitBinaryKernel(struct SedonaCScalarKernel* out, const char* name) {
  auto* data = new KernelData();
  data->name = name;
  out->private_data = data;
  out->function_name = &KernelFunctionName;
  out->new_impl = &SedonaBinaryKernelAdapter<Exec>::NewImpl;
  out->release = &KernelRelease;
}

/// @}

}  // namespace sedona_udf

}  // namespace s2geography
