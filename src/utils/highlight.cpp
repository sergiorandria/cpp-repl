#include "cpp-repl/utils/highlight.h"

#include <cctype>
#include <unordered_set>

namespace cpprepl {
namespace utils {

static const std::unordered_set<std::string> kKeywords = {
    "alignas",       "alignof",     "and",
    "and_eq",        "asm",         "auto",
    "bitand",        "bitor",       "bool",
    "break",         "case",        "catch",
    "char",          "char8_t",     "char16_t",
    "char32_t",      "class",       "compl",
    "concept",       "const",       "consteval",
    "constexpr",     "constinit",   "const_cast",
    "continue",      "co_await",    "co_return",
    "co_yield",      "decltype",    "default",
    "delete",        "do",          "double",
    "dynamic_cast",  "else",        "enum",
    "explicit",      "export",      "extern",
    "false",         "float",       "for",
    "friend",        "goto",        "if",
    "inline",        "int",         "long",
    "mutable",       "namespace",   "new",
    "noexcept",      "not",         "not_eq",
    "nullptr",       "operator",    "or",
    "or_eq",         "private",     "protected",
    "public",        "register",    "reinterpret_cast",
    "requires",      "return",      "short",
    "signed",        "sizeof",      "static",
    "static_assert", "static_cast", "struct",
    "switch",        "template",    "this",
    "thread_local",  "throw",       "true",
    "try",           "typedef",     "typeid",
    "typename",      "union",       "unsigned",
    "using",         "virtual",     "void",
    "volatile",      "wchar_t",     "while",
    "xor",           "xor_eq",      "import",
    "module"};

static const std::unordered_set<std::string> kTypes = {
    "int",         "float",    "double",  "char",   "bool",    "void",        "long",
    "short",       "unsigned", "signed",  "size_t", "auto",    "std::string", "string",
    "std::vector", "std::map", "cpp_int", "bigint", "mpz_int", "int64_t",     "uint64_t"};

static const std::unordered_set<std::string> kConsts = {"true", "false", "nullptr"};

std::string Highlighter::highlight(const std::string &code, bool useColor) {
  if (!useColor || code.empty())
    return code;
  const std::string RST = "\033[0m";
  const std::string GREY = "\033[90m";
  const std::string YELLOW = "\033[33m";
  const std::string MAGENTA = "\033[95m";
  const std::string CYAN = "\033[36m";
  const std::string B_CYAN = "\033[1;36m";
  const std::string GREEN = "\033[32m";
  const std::string B_MAGENTA = "\033[1;35m";
  const std::string DIM = "\033[2m";
  // Preprocessor yellow bright
  std::string out;
  out.reserve(code.size() * 2);
  size_t i = 0, n = code.size();
  bool atLineStart = true;
  while (i < n) {
    char c = code[i];
    char nxt = (i + 1 < n) ? code[i + 1] : '\0';
    // handle // comment
    if (c == '/' && nxt == '/') {
      size_t j = i;
      while (j < n && code[j] != '\n')
        ++j;
      out += GREY + code.substr(i, j - i) + RST;
      i = j;
      atLineStart = false;
      continue;
    }
    if (c == '/' && nxt == '*') {
      size_t j = i + 2;
      while (j + 1 < n && !(code[j] == '*' && code[j + 1] == '/'))
        ++j;
      if (j + 1 < n)
        j += 2;
      else
        j = n;
      out += GREY + code.substr(i, j - i) + RST;
      i = j;
      continue;
    }
    if (c == '"') {
      size_t j = i + 1;
      bool esc = false;
      while (j < n) {
        if (esc)
          esc = false;
        else if (code[j] == '\\')
          esc = true;
        else if (code[j] == '"') {
          ++j;
          break;
        }
        ++j;
      }
      out += GREEN + code.substr(i, j - i) + RST;
      i = j;
      atLineStart = false;
      continue;
    }
    if (c == '\'') {
      size_t j = i + 1;
      bool esc = false;
      while (j < n) {
        if (esc)
          esc = false;
        else if (code[j] == '\\')
          esc = true;
        else if (code[j] == '\'') {
          ++j;
          break;
        }
        ++j;
      }
      out += GREEN + code.substr(i, j - i) + RST;
      i = j;
      atLineStart = false;
      continue;
    }
    if (atLineStart && c == '#') {
      // preprocessor line till \n
      size_t j = i;
      while (j < n && code[j] != '\n')
        ++j;
      out += YELLOW + code.substr(i, j - i) + RST;
      i = j;
      continue;
    }
    if (c == '\n') {
      out += c;
      ++i;
      atLineStart = true;
      continue;
    }
    if (std::isspace((unsigned char)c)) {
      out += c;
      ++i;
      continue;
    }
    // number
    if (std::isdigit((unsigned char)c) || (c == '.' && std::isdigit((unsigned char)nxt))) {
      size_t j = i;
      while (j < n && (std::isalnum((unsigned char)code[j]) || code[j] == '.' || code[j] == '_'))
        ++j;
      out += YELLOW + code.substr(i, j - i) + RST;
      i = j;
      atLineStart = false;
      continue;
    }
    // word
    if (std::isalpha((unsigned char)c) || c == '_' || c == ':') {
      size_t j = i;
      while (j < n && (std::isalnum((unsigned char)code[j]) || code[j] == '_' || code[j] == ':'))
        ++j;
      std::string w = code.substr(i, j - i);
      // strip leading :: ?
      std::string core = w;
      // check exact
      if (kConsts.count(w)) {
        out += B_MAGENTA + w + RST;
      } else if (kTypes.count(w)) {
        out += B_CYAN + w + RST;
      } else if (kKeywords.count(w)) {
        out += MAGENTA + w + RST;
      } else {
        out += w;
      }
      i = j;
      atLineStart = false;
      continue;
    }
    out += c;
    ++i;
    atLineStart = false;
  }
  return out;
}

std::string Highlighter::highlightType(const std::string &typeStr, bool useColor) {
  if (!useColor)
    return typeStr;
  return std::string("\033[36m") + typeStr + "\033[0m";
}

std::string Highlighter::highlightValue(const std::string &valStr, bool useColor) {
  if (!useColor)
    return valStr;
  // numbers green, strings yellow, etc. simple: if val starts with " or contains digits, color
  if (valStr.empty())
    return valStr;
  if (valStr.front() == '"' || valStr.front() == '\'') {
    return std::string("\033[32m") + valStr + "\033[0m";
  }
  // numeric?
  bool isNum = true;
  for (char c : valStr)
    if (!std::isdigit((unsigned char)c) && c != '.' && c != '-' && c != 'e' && c != 'E' &&
        c != '+' && c != 'f' && c != 'L') {
      isNum = false;
      break;
    }
  if (isNum)
    return std::string("\033[33m") + valStr + "\033[0m";
  return std::string("\033[32m") + valStr + "\033[0m";
}

} // namespace utils
} // namespace cpprepl
