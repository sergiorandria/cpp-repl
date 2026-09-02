#pragma once
#include <string>
#include <vector>

namespace cpprepl {
namespace utils {

/**
 * @file version_detector.h
 * @brief Automatic C++ standard detection.
 */

/**
 * @brief Supported C++ standards.
 */
enum class StdVersion {
  Cpp17 = 17, ///< C++17
  Cpp20 = 20, ///< C++20
  Cpp23 = 23  ///< C++23
};

/**
 * @brief Detects required C++ standard from source keywords.
 */
class VersionDetector {
public:
  /**
   * @brief Detect standard needed for given code.
   * @param code Source snippet to inspect.
   * @return Required StdVersion (minimum is Cpp17).
   */
  static StdVersion detect(const std::string &code) noexcept;

  /** @brief Convert version to compiler flag, e.g. -std=c++20. */
  static std::string toFlag(StdVersion v) noexcept;
  /** @brief Convert version to human string, e.g. C++20. */
  static std::string toString(StdVersion v) noexcept;
  /** @brief Human description with features. */
  static std::string describe(StdVersion v);

private:
  static bool contains(const std::string &code, const std::string &kw);
  static bool containsWord(const std::string &code, const std::string &word);
};

/**
 * @brief Return the maximum of two versions.
 */
StdVersion maxVersion(StdVersion a, StdVersion b);

} // namespace utils
} // namespace cpprepl
