#pragma once
#include <string>
#include <vector>

namespace cpprepl {
namespace utils {

enum class StdVersion {
  Cpp17 = 17,
  Cpp20 = 20,
  Cpp23 = 23
};

class VersionDetector {
public:
  // Auto-detect required C++ standard from code keywords.
  // Like python's future imports, but for C++.
  static StdVersion detect(const std::string &code);

  static std::string toFlag(StdVersion v);
  static std::string toString(StdVersion v);
  static std::string describe(StdVersion v);

private:
  static bool contains(const std::string &code, const std::string &kw);
  static bool containsWord(const std::string &code, const std::string &word);
};

// Helper to get current interpreter version as string for diagnostics
StdVersion maxVersion(StdVersion a, StdVersion b);

} // namespace utils
} // namespace cpprepl
