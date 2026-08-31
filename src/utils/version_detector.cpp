#include "cpp-repl/utils/version_detector.h"
#include <algorithm>

namespace cpprepl {
namespace utils {

static std::string stripCommentsAndStrings(const std::string &code) {
  std::string out;
  out.reserve(code.size());
  bool inLineComment = false, inBlockComment = false, inString = false, inChar = false, escape = false;
  for (size_t i = 0; i < code.size(); ++i) {
    char c = code[i];
    char nxt = (i+1 < code.size()) ? code[i+1] : '\0';
    if (inLineComment) {
      if (c == '\n') { inLineComment = false; out.push_back(c); }
      continue;
    }
    if (inBlockComment) {
      if (c == '*' && nxt == '/') { inBlockComment = false; ++i; }
      continue;
    }
    if (inString) {
      if (!escape && c == '"') inString = false;
      escape = !escape && c == '\\';
      out.push_back(' '); // keep placeholder to preserve word boundaries
      continue;
    }
    if (inChar) {
      if (!escape && c == '\'') inChar = false;
      escape = !escape && c == '\\';
      out.push_back(' ');
      continue;
    }
    if (c == '/' && nxt == '/') { inLineComment = true; ++i; continue; }
    if (c == '/' && nxt == '*') { inBlockComment = true; ++i; continue; }
    if (c == '"') { inString = true; out.push_back(' '); continue; }
    if (c == '\'') { inChar = true; out.push_back(' '); continue; }
    out.push_back(c);
  }
  return out;
}

bool VersionDetector::contains(const std::string &code, const std::string &kw) {
  return code.find(kw) != std::string::npos;
}
bool VersionDetector::containsWord(const std::string &code, const std::string &word) {
  // naive word boundary check
  size_t pos = 0;
  while ((pos = code.find(word, pos)) != std::string::npos) {
    bool leftOk = pos == 0 || (!std::isalnum((unsigned char)code[pos-1]) && code[pos-1] != '_');
    bool rightOk = pos + word.size() == code.size() || (!std::isalnum((unsigned char)code[pos+word.size()]) && code[pos+word.size()] != '_');
    if (leftOk && rightOk) return true;
    pos += word.size();
  }
  return false;
}

StdVersion VersionDetector::detect(const std::string &code) {
  std::string stripped = stripCommentsAndStrings(code);
  // C++23 keywords – check stripped
  if (contains(stripped, "import ") || contains(stripped, "module ") ||
      containsWord(stripped, "import") || containsWord(stripped, "export")) {
    if (stripped.find("import") != std::string::npos) return StdVersion::Cpp23;
  }
  // C++20 keywords
  if (containsWord(stripped, "concept") || containsWord(stripped, "requires") ||
      containsWord(stripped, "co_await") || containsWord(stripped, "co_yield") ||
      containsWord(stripped, "co_return") || containsWord(stripped, "char8_t") ||
      contains(stripped, "<=>") || containsWord(stripped, "consteval") ||
      containsWord(stripped, "constinit")) {
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
