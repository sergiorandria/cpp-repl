#pragma once
#include <string>
#include <variant>
#include <optional>

namespace cpprepl {
namespace utils {

// Lightweight Result<T> pattern (like std::expected) to avoid bool+string err anti-pattern.
// Keeps error context and forces caller to handle errors.
template <typename T>
class Result {
public:
  static Result success(T v) { return Result(std::move(v)); }
  static Result failure(std::string e) { return Result(std::move(e)); }
  // For void specialization
  static Result success() { return Result(true); }

  bool ok() const { return std::holds_alternative<T>(data_); }
  explicit operator bool() const { return ok(); }
  const T& value() const { return std::get<T>(data_); }
  T& value() { return std::get<T>(data_); }
  const std::string& error() const { return std::get<std::string>(data_); }

private:
  explicit Result(T v) : data_(std::move(v)) {}
  explicit Result(std::string e) : data_(std::move(e)) {}
  explicit Result(bool) : data_(std::string("")) {} // for void
  std::variant<T, std::string> data_;
};

template <>
class Result<void> {
public:
  static Result success() { return Result(true); }
  static Result failure(std::string e) { return Result(std::move(e)); }
  bool ok() const { return ok_; }
  explicit operator bool() const { return ok_; }
  const std::string& error() const { return err_; }
private:
  explicit Result(bool ok) : ok_(ok) {}
  explicit Result(std::string e) : ok_(false), err_(std::move(e)) {}
  bool ok_ = true;
  std::string err_;
};

} // namespace utils
} // namespace cpprepl
