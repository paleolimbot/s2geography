#pragma once

#include <string>
#include <string_view>

namespace s2geography {

namespace op {

/// \defgroup operator S2 Operators
///
/// S2 Operators are simplifications on top of the S2 library to
/// reduce the amount of glue code required to bind operations in
/// other runtimes. These abstractions intentionally do not include
/// the s2 headers publicly.
///
/// The general workflow for using an Op class is to (1) create it
/// with the appropriate options class, (2) call Init() to give
/// the Op implementation an opportunity to error for invalid Options,
/// and (3) loop and call ExecuteScalar() where appropriate. Future
/// extensions to this framework may add an opportunity for operators
/// to implement a fast path to loop over arrays (or ArrowArrays)
/// of input.
///
/// @{

struct EmptyOptions {};

template <typename RetT, typename ArgT0, typename OptT = EmptyOptions>
class UnaryOp {
 public:
  using ReturnT = RetT;
  using ArgType0 = ArgT0;
  using OptionsT = OptT;

  UnaryOp(const OptT& options = OptT()) : options_(options) {}
  virtual void Init() {}
  virtual ReturnT ExecuteScalar(const ArgType0) { return ReturnT{}; }

 protected:
  OptT options_;
};

template <typename RetT, typename ArgT0, typename ArgT1,
          typename OptT = EmptyOptions>
class BinaryOp {
 public:
  using ReturnT = RetT;
  using ArgType0 = ArgT0;
  using ArgType1 = ArgT1;
  using OptionsT = OptT;

  BinaryOp(const OptionsT& options = OptionsT()) : options_(options) {}
  virtual void Init() {}
  virtual ReturnT ExecuteScalar(const ArgType0, const ArgType1) {
    return ReturnT{};
  }

 protected:
  OptionsT options_;
};

template <typename Op>
typename Op::ReturnT Execute(typename Op::ArgType0 arg0) {
  Op op;
  op.Init();

  return op.ExecuteScalar(arg0);
}

template <typename Op>
typename Op::ReturnT Execute(typename Op::ArgType0 arg0,
                             typename Op::ArgType1 arg1) {
  Op op;
  op.Init();

  return op.ExecuteScalar(arg0, arg1);
}

// This overload allow executing functions that return std::string_view,
// since the regular Execute() will return a view to deleted memory.
template <typename Op>
std::string ExecuteString(typename Op::ArgType0 arg0) {
  Op op;
  op.Init();

  return std::string(op.ExecuteScalar(arg0));
}

/// @}

}  // namespace op

}  // namespace s2geography
