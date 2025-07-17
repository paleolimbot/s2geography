#pragma once

#include <cerrno>
#include <unordered_map>

#include "geoarrow/geoarrow.hpp"
#include "nanoarrow/nanoarrow.hpp"
#include "s2geography.h"
#include "s2geography/arrow_udf/arrow_udf.h"

namespace s2geography {

namespace arrow_udf {

/// \brief Friendlier UDF wrapper
///
/// The user-facing ArrowUDF is designed to be C-friendly; however, its
/// signatures make it difficult to leverage C++ idoms to create an
/// implementation. This abstract class provides a slightly nicer interface for
/// implementors.
class InternalUDF : public ArrowUDF {
 public:
  int Init(struct ArrowSchema *arg_schema, const char *options,
           struct ArrowSchema *out) override {
    last_error_.clear();
    try {
      if (arg_schema == nullptr || arg_schema->release == nullptr) {
        last_error_ = "Invalid or released arg_schema";
        return EINVAL;
      }

      // Consume schemas
      for (int64_t i = 0; i < arg_schema->n_children; i++) {
        arg_types_.emplace_back(arg_schema->children[i]);
      }

      // Parse options
      struct ArrowMetadataReader reader;
      struct ArrowStringView k, v;
      NANOARROW_THROW_NOT_OK(ArrowMetadataReaderInit(&reader, options));
      while (reader.remaining_keys > 0) {
        NANOARROW_THROW_NOT_OK(ArrowMetadataReaderRead(&reader, &k, &v));
        options_.insert(
            {std::string(k.data, static_cast<size_t>(k.size_bytes)),
             std::string(v.data, static_cast<size_t>(k.size_bytes))});
      }

      auto return_type = ReturnType();
      ArrowSchemaMove(return_type.get(), out);

      return GEOARROW_OK;
    } catch (std::exception &e) {
      last_error_ = e.what();
      return EINVAL;
    }
  }

  int Execute(struct ArrowArray **args, int64_t n_args,
              struct ArrowArray *out) override {
    last_error_.clear();
    try {
      std::vector<nanoarrow::UniqueArray> arg_vec;
      for (int64_t i = 0; i < n_args; i++) {
        arg_vec.emplace_back(args[i]);
      }

      auto result = ExecuteImpl(arg_vec);
      ArrowArrayMove(result.get(), out);

      return GEOARROW_OK;
    } catch (std::exception &e) {
      last_error_ = e.what();
      return EINVAL;
    }
  }

  const char *GetLastError() override { return last_error_.c_str(); }

 protected:
  /// \brief Argument types populated by this class before calling ReturnType()
  std::vector<nanoarrow::UniqueSchema> arg_types_;

  /// \brief Parsed options if any were provided, populated by this class before
  /// calling ReturnType().
  std::unordered_map<std::string, std::string> options_;

  /// \brief Validate input and calculate a return type
  virtual nanoarrow::UniqueSchema ReturnType() = 0;

  /// \brief Execute a single batch
  virtual nanoarrow::UniqueArray ExecuteImpl(
      const std::vector<nanoarrow::UniqueArray> &args) = 0;

 private:
  std::string last_error_;
};

/// \defgroup arrow_udf-utils Arrow UDF Utilities
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

  void InitOutputType(struct ArrowSchema *out) {
    NANOARROW_THROW_NOT_OK(ArrowSchemaInitFromType(out, arrow_type_val));
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
      static_assert(false, "value type not supported");
    }
  }

  void Finish(struct ArrowArray *out) {
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
  using c_type = const Geography &;

  WkbGeographyOutputBuilder() {
    writer_.Init(geoarrow::Writer::OutputType::kWKB, geoarrow::ExportOptions());
  }

  void InitOutputType(struct ArrowSchema *out) {
    ::geoarrow::Wkb().InitSchema(out);
  }

  void Reserve(int64_t additional_size) {
    // The current geoarrow writer doesn't provide any support for this
  }

  void AppendNull() { writer_.WriteNull(); }

  void Append(c_type value) { writer_.WriteGeography(value); }

  void Finish(struct ArrowArray *out) { writer_.Finish(out); }

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

  ArrowInputView(const struct ArrowSchema *type) {
    NANOARROW_THROW_NOT_OK(
        ArrowArrayViewInitFromSchema(view_.get(), type, nullptr));
  }

  void SetArray(const struct ArrowArray *array, int64_t num_rows) {
    NANOARROW_THROW_NOT_OK(ArrowArrayViewSetArray(view_.get(), array, nullptr));

    if (array->length == 0) {
      throw Exception("Array input must not be empty");
    }
  }

  bool IsNull(int64_t i) {
    return ArrowArrayViewIsNull(view_.get(), i % view_->length);
  }

  c_type_t Get(int64_t i) {
    if constexpr (std::is_integral_v<c_type_t>) {
      return ArrowArrayViewGetIntUnsafe(view_.get(), i % view_->length);
    } else if constexpr (std::is_floating_point_v<c_type>) {
      return ArrowArrayViewGetDoubleUnsafe(view_.get(), i % view_->length);
    } else {
      static_assert(false, "value type not supported");
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
  using c_type = const Geography &;

  GeographyInputView(const struct ArrowSchema *type)
      : current_array_(nullptr), stashed_index_(-1) {
    reader_.Init(type);
  }

  void SetArray(const struct ArrowArray *array, int64_t num_rows) {
    current_array_ = array;
    stashed_index_ = -1;
  }

  bool IsNull(int64_t i) {
    StashIfNeeded(i % current_array_->length);
    return stashed_[0].get() == nullptr;
  }

  const Geography &Get(int64_t i) {
    StashIfNeeded(i % current_array_->length);
    return *stashed_[0];
  }

 private:
  geoarrow::Reader reader_;
  const struct ArrowArray *current_array_;
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
  using c_type = const ShapeIndexGeography &;

  GeographyIndexInputView(const struct ArrowSchema *type)
      : inner_(type), stashed_index_(-1) {}

  void SetArray(const struct ArrowArray *array, int64_t num_rows) {
    stashed_index_ = -1;
    inner_.SetArray(array, num_rows);
    current_array_length_ = array->length;
  }

  bool IsNull(int64_t i) { return inner_.IsNull(i); }

  const ShapeIndexGeography &Get(int64_t i) {
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
      const auto &geog = inner_.Get(i);
      stashed_ = ShapeIndexGeography(geog);
      stashed_index_ = i;
    }
  }
};

/// \brief ArrowUDF implementation for unary functions
///
/// This class is templated on an Exec, which provides type parameters
/// denoting the input view class and output builder class. Exec::Init()
/// is called to provide the implementation the options, and Exec() is
/// called for each non-null scalar input. This implementation always
/// propagates nulls from input to output.
template <typename Exec>
class UnaryUDF : public InternalUDF {
 protected:
  using arg0_t = typename Exec::arg0_t;
  using out_t = typename Exec::out_t;

  nanoarrow::UniqueSchema ReturnType() override {
    if (arg_types_.size() != 1) {
      throw Exception("Expected one argument in unary s2geography UDF");
    }

    arg0 = std::make_unique<arg0_t>(arg_types_[0].get());
    out = std::make_unique<out_t>();
    exec.Init(options_);

    nanoarrow::UniqueSchema out_type;
    out->InitOutputType(out_type.get());
    return out_type;
  }

  nanoarrow::UniqueArray ExecuteImpl(
      const std::vector<nanoarrow::UniqueArray> &args) override {
    if (args.size() != 1 || arg_types_.size() != 1) {
      throw Exception(
          "Expected one argument/one argument type in in unary s2geography "
          "UDF");
    }

    int64_t num_rows = args[0]->length;
    arg0->SetArray(args[0].get(), num_rows);
    out->Reserve(num_rows);

    for (int i = 0; i < num_rows; i++) {
      if (arg0->IsNull(i)) {
        out->AppendNull();
      } else {
        typename Exec::arg0_t::c_type item0 = arg0->Get(i);
        typename Exec::out_t::c_type item_out = exec.Exec(item0);
        out->Append(item_out);
      }
    }

    nanoarrow::UniqueArray array_out;
    out->Finish(array_out.get());
    return array_out;
  }

 private:
  std::unique_ptr<arg0_t> arg0;
  std::unique_ptr<out_t> out;
  Exec exec;
};

/// \brief ArrowUDF implementation for binary functions
///
/// This class is templated on an Exec, which provides type parameters
/// denoting the input view classes and output builder class. Exec::Init()
/// is called to provide the implementation the options, and Exec() is
/// called for each non-null scalar input. This implementation always
/// propagates nulls from input to output.
template <typename Exec>
class BinaryUDF : public InternalUDF {
 protected:
  using arg0_t = typename Exec::arg0_t;
  using arg1_t = typename Exec::arg1_t;
  using out_t = typename Exec::out_t;

  nanoarrow::UniqueSchema ReturnType() override {
    if (arg_types_.size() != 2) {
      throw Exception("Expected one argument in unary s2geography UDF");
    }

    arg0 = std::make_unique<arg0_t>(arg_types_[0].get());
    arg1 = std::make_unique<arg1_t>(arg_types_[1].get());
    out = std::make_unique<out_t>();
    exec.Init(options_);

    nanoarrow::UniqueSchema out_type;
    out->InitOutputType(out_type.get());
    return out_type;
  }

  nanoarrow::UniqueArray ExecuteImpl(
      const std::vector<nanoarrow::UniqueArray> &args) override {
    if (args.size() != 2 || arg_types_.size() != 2) {
      throw Exception(
          "Expected one argument/one argument type in in unary s2geography "
          "UDF");
    }

    int64_t num_rows = 1;
    for (const auto &arg : args) {
      if (arg->length != 1) {
        num_rows = arg->length;
        break;
      }
    }

    arg0->SetArray(args[0].get(), num_rows);
    arg1->SetArray(args[1].get(), num_rows);
    out->Reserve(num_rows);

    for (int i = 0; i < num_rows; i++) {
      if (arg0->IsNull(i) || arg1->IsNull(i)) {
        out->AppendNull();
      } else {
        typename Exec::arg0_t::c_type item0 = arg0->Get(i);
        typename Exec::arg1_t::c_type item1 = arg1->Get(i);
        typename Exec::out_t::c_type item_out = exec.Exec(item0, item1);
        out->Append(item_out);
      }
    }

    nanoarrow::UniqueArray array_out;
    out->Finish(array_out.get());
    return array_out;
  }

 private:
  std::unique_ptr<arg0_t> arg0;
  std::unique_ptr<arg1_t> arg1;
  std::unique_ptr<out_t> out;
  Exec exec;
};

/// @}

}  // namespace arrow_udf

}  // namespace s2geography
