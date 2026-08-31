#include "cpp-repl/utils/version_detector.h"
#include <algorithm>

namespace cpprepl {
namespace utils {

bool VersionDetector::contains(const std::string &code, const std::string &kw) {
  return code.find(kw) != std::string::npos;
}
bool VersionDetector::containsWord(const std::string &code, const std::string &word) {
  // naive word boundary check
  size_t pos = 0;
  while ((pos = code.find(word, pos)) != std::string::npos) {
    bool leftOk = pos == 0 || !std::isalnum(code[pos-1]) && code[pos-1] != '_';
    bool rightOk = pos + word.size() == code.size() || !std::isalnum(code[pos+word.size()]) && code[pos+word.size()] != '_';
    if (leftOk && rightOk) return true;
    pos += word.size();
  }
  return false;
}

StdVersion VersionDetector::detect(const std::string &code) {
  // C++23 keywords
  if (contains(code, "import ") || contains(code, "module ") ||
      containsWord(code, "import") || containsWord(code, "export")) {
    // import <...> ; or import foo;
    if (code.find("import") != std::string::npos) return StdVersion::Cpp23;
  }
  // C++20 keywords
  if (containsWord(code, "concept") || containsWord(code, "requires") ||
      containsWord(code, "co_await") || containsWord(code, "co_yield") ||
      containsWord(code, "co_return") || containsWord(code, "char8_t") ||
      contains(code, "<=>") || containsWord(code, "consteval") ||
      containsWord(code, "constinit")) {
    return StdVersion::Cpp20;
  }
  return StdVersion::Cpp17;
}

std::string VersionDetector::toFlag(StdVersion v) {
  switch(v) {
    case StdVersion::Cpp17: return "-std=c++17";
    case StdVersion::Cpp20: return "-std=c++20";
    case StdVersion::Cpp23: return "-std=c++23";
  }
  return "-std=c++17";
}
std::string VersionDetector::toString(StdVersion v) {
  switch(v) {
    case StdVersion::Cpp17: return "C++17";
    case StdVersion::Cpp20: return "C++20";
    case StdVersion::Cpp23: return "C++23";
  }
  return "C++17";
}
std::string VersionDetector::describe(StdVersion v) {
  return toString(v) + " (" + toFlag(v) + ")";
}

StdVersion maxVersion(StdVersion a, StdVersion b) {
  return (static_cast<int>(a) > static_cast<int>(b)) ? a : b;
}

} // namespace utils
} // namespace cpprepl
