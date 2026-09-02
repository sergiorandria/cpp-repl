#pragma once
#include <optional>
#include <string>
#include <variant>

/**
 * @file result.h
 * @brief Result<T> — std::expected polyfill for explicit error handling.
 * @details Avoids bool+string anti-pattern (AGENTS.md §4). Forces callers to
 *          handle errors via ok()/error()/value(). Primary API for Interpreter
 *          and Session; legacy bool+string kept for compat and to be deprecated.
 */

namespace cpprepl {
namespace utils {

/**
 * @brief Result with value or error string.
 * @tparam T Value type on success.
 * @details On success holds T, on failure holds error string. Use ok()/operator bool() to check.
 */
template <typename T> class Result {
public:
  static Result success(T v) {
    return Result(std::move(v));
  }
  static Result failure(std::string e) {
    return Result(std::move(e));
  }
  // For void specialization
  static Result success() {
    return Result(true);
  }

  bool ok() const {
    return std::holds_alternative<T>(data_);
  }
  explicit operator bool() const {
    return ok();
  }
  const T &value() const {
    return std::get<T>(data_);
  }
  T &value() {
    return std::get<T>(data_);
  }
  const std::string &error() const {
    return std::get<std::string>(data_);
  }

private:
  explicit Result(T v) : data_(std::move(v)) {}
  explicit Result(std::string e) : data_(std::move(e)) {}
  explicit Result(bool) : data_(std::string("")) {} // for void
  std::variant<T, std::string> data_;
};

template <> class Result<void> {
public:
  static Result success() {
    return Result(true);
  }
  static Result failure(std::string e) {
    return Result(std::move(e));
  }
  bool ok() const {
    return ok_;
  }
  explicit operator bool() const {
    return ok_;
  }
  const std::string &error() const {
    return err_;
  }

private:
  explicit Result(bool ok) : ok_(ok) {}
  explicit Result(std::string e) : ok_(false), err_(std::move(e)) {}
  bool ok_ = true;
  std::string err_;
};

} // namespace utils
} // namespace cpprepl
