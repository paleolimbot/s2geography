#pragma once

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

template <typename ReturnT, typename ArgType0, typename OptionsT = EmptyOptions>
class UnaryOp {
 public:
  UnaryOp(const OptionsT& options = OptionsT()) : options_(options) {}
  virtual void Init() {}
  virtual ReturnT ExecuteScalar(const ArgType0 arg0) { return ReturnT(); }

 protected:
  OptionsT options_;
};

template <typename ReturnT, typename ArgType0, typename ArgType1,
          typename OptionsT = EmptyOptions>
class BinaryOp {
 public:
  BinaryOp(const OptionsT& options = OptionsT()) : options_(options) {}
  virtual void Init() {}
  virtual ReturnT ExecuteScalar(const ArgType0 arg0, const ArgType1 arg1) {}

 protected:
  OptionsT options_;
};

/// @}

}  // namespace op

}  // namespace s2geography
