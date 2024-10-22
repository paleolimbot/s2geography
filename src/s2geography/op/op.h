#pragma once

namespace s2geography {

namespace op {

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

}  // namespace op

}  // namespace s2geography
