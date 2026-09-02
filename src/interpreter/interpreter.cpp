/**
 * @file interpreter.cpp
 * @brief Interpreter implementation with high-precision float printing and BigInt handling.
 */
#include "cpp-repl/interpreter/interpreter.h"
#include "cpp-repl/utils/bigint.h"
#include "cpp-repl/utils/highlight.h"
#include "cpp-repl/utils/incomplete_detector.h"
#include "cpp-repl/utils/version_detector.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Interpreter/Interpreter.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/AST/Type.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <algorithm>
#include <limits>
#include <cmath>
#include <cstdlib>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace {
/**
 * @brief Helper for high-precision floating point printing.
 *
 * Replaces clang's default %.6g/%.8g which truncates to 6-8 digits.
 * Uses max_digits10 (9 for float, 17 for double, 21 for long double)
 * so printed values round-trip and preserve input precision.
 */
static std::string formatFloatHighPrec(float v) {
  std::string out;
  llvm::raw_string_ostream ss(out);
  if (std::isnan(v) || std::isinf(v)) {
    ss << llvm::format("%g", v);
  } else if (v == static_cast<float>(static_cast<int64_t>(v))) {
    ss << llvm::format("%.1f", v);
  } else {
    ss << llvm::format("%#.9g", v);
  }
  ss << 'f';
  return ss.str();
}
static std::string formatDoubleHighPrec(double v) {
  std::string out;
  llvm::raw_string_ostream ss(out);
  if (std::isnan(v) || std::isinf(v)) {
    ss << llvm::format("%g", v);
  } else if (v == static_cast<double>(static_cast<int64_t>(v))) {
    ss << llvm::format("%.1f", v);
  } else {
    ss << llvm::format("%#.17g", v);
  }
  return ss.str();
}
static std::string formatLongDoubleHighPrec(long double v) {
  std::string out;
  llvm::raw_string_ostream ss(out);
  if (std::isnan(v) || std::isinf(v)) {
    ss << llvm::format("%Lg", v);
  } else if (v == static_cast<long double>(static_cast<int64_t>(v))) {
    ss << llvm::format("%.1Lf", v);
  } else {
    constexpr int prec = std::numeric_limits<long double>::max_digits10;
    std::string fmt = "%#." + std::to_string(prec) + "Lg";
    ss << llvm::format(fmt.c_str(), v);
  }
  ss << 'L';
  return ss.str();
}

static void highPrecisionDump(const clang::Value &V) {
  if (!V.isValid() || V.isVoid())
    return;
  std::string typeStr;
  {
    llvm::raw_string_ostream ts(typeStr);
    V.printType(ts);
  }
  std::string dataStr;
  bool handled = false;
  // Try direct Kind first – covers most prvalues
  switch (V.getKind()) {
  case clang::Value::K_Float:
    dataStr = formatFloatHighPrec(V.getFloat());
    handled = true;
    break;
  case clang::Value::K_Double:
    dataStr = formatDoubleHighPrec(V.getDouble());
    handled = true;
    break;
  case clang::Value::K_LongDouble:
    dataStr = formatLongDoubleHighPrec(V.getLongDouble());
    handled = true;
    break;
  default:
    break;
  }
  if (!handled) {
    // Fallback: check QualType for reference / object cases where Kind is
    // K_PtrOrObj but the underlying type is a builtin floating type.
    clang::QualType qt = V.getType();
    clang::QualType nonRef = qt.getNonReferenceType();
    const clang::Type *canon = nonRef.getCanonicalType().getTypePtr();
    if (auto *bt = llvm::dyn_cast<clang::BuiltinType>(canon)) {
      if (bt->getKind() == clang::BuiltinType::Float ||
          bt->getKind() == clang::BuiltinType::Double ||
          bt->getKind() == clang::BuiltinType::LongDouble) {
        // Value is stored as a pointer (reference / object)
        if (V.getKind() == clang::Value::K_PtrOrObj && V.getPtr()) {
          void *p = V.getPtr();
          if (bt->getKind() == clang::BuiltinType::Float) {
            float fv = *static_cast<float *>(p);
            dataStr = formatFloatHighPrec(fv);
            handled = true;
          } else if (bt->getKind() == clang::BuiltinType::Double) {
            double dv = *static_cast<double *>(p);
            dataStr = formatDoubleHighPrec(dv);
            handled = true;
          } else {
            long double ldv = *static_cast<long double *>(p);
            dataStr = formatLongDoubleHighPrec(ldv);
            handled = true;
          }
        }
      }
    }
  }
  if (!handled) {
    llvm::raw_string_ostream ds(dataStr);
    V.printData(ds);
  }
  // Keyword highlight: colorize type and value when tty and color enabled
  bool useColor = false;
#ifndef _WIN32
  useColor = isatty(STDOUT_FILENO) && !getenv("NO_COLOR") && !getenv("CPP_REPL_NO_COLOR") && !getenv("NO_COLOUR");
  if (useColor) {
    const char *term = getenv("TERM");
    if (term && std::string(term)=="dumb") useColor=false;
  }
  if (getenv("FORCE_COLOR") || getenv("CLICOLOR_FORCE")) useColor = true;
#else
  useColor = false;
#endif
  if (getenv("CPP_REPL_NO_COLOR") || getenv("NO_COLOR")) useColor = false;
  if (useColor) {
    std::string colVal = cpprepl::utils::Highlighter::highlightValue(dataStr, true);
    llvm::outs() << "\033[90m[result]\033[0m (\033[36m" << typeStr << "\033[0m) " << colVal << "\n";
  } else {
    llvm::outs() << "[result] (" << typeStr << ") " << dataStr << "\n";
  }
}
} // namespace

namespace cpprepl {
namespace interpreter {

Interpreter::Interpreter() : tracker_(utils::VariableTrackerFactory::create()) {}
Interpreter::~Interpreter() = default;

bool Interpreter::init(utils::StdVersion version,
                   const std::vector<std::string> &includePaths,
                   const std::vector<std::string> &defines, std::string &err) {
  return init(version, includePaths, defines, {}, {}, err);
}

bool Interpreter::init(utils::StdVersion version, std::string &err) {
  return init(version, {}, {}, err);
}

bool Interpreter::init(utils::StdVersion version,
                   const std::vector<std::string> &includePaths,
                   const std::vector<std::string> &defines,
                   const std::vector<std::string> &libraryPaths,
                   const std::vector<std::string> &libraries,
                   std::string &err) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  // Store for re-init (version upgrade, :I, etc.)
  includePaths_ = includePaths;
  defines_ = defines;
  libraryPaths_ = libraryPaths;
  libraries_ = libraries;
  currentVersion_ = version;
  stdLibIncluded_ = false;

  clang::IncrementalCompilerBuilder builder;
  compilerArgsStorage_.clear();
  compilerArgsStorage_.push_back(utils::VersionDetector::toFlag(version));
  compilerArgsStorage_.push_back("-O0");
  compilerArgsStorage_.push_back("-resource-dir");
#ifndef CLANG_RESOURCE_DIR
#define CLANG_RESOURCE_DIR "/usr/lib/clang/22"
#endif
  {
    std::string resDir = CLANG_RESOURCE_DIR;
    // Runtime fallback: if configured dir doesn't exist, probe common layouts
    std::error_code ec;
    if (!std::filesystem::exists(resDir, ec)) {
      const std::vector<std::string> candidates = {
        "/usr/lib/llvm-22/lib/clang/22",
        "/usr/lib/clang/22",
        "/usr/lib/llvm/lib/clang/22",
        "/usr/lib/llvm-22/lib/clang/22"
      };
      for (auto &c : candidates) {
        if (std::filesystem::exists(c, ec)) { resDir = c; break; }
      }
    }
    compilerArgsStorage_.push_back(resDir);
  }
  // Fix for Numpy-C-API headers (NZERO, vector<bool>, ProxyBase) without modifying them
  // Make -include conditional: only add if header actually exists, otherwise skip (prevents fatal error in CI artifact)
#ifndef CPP_REPL_INCLUDE_DIR
#define CPP_REPL_INCLUDE_DIR "/usr/include"
#endif
  {
    std::string actualInc;
    std::string projInc = CPP_REPL_INCLUDE_DIR;
    std::error_code ec;
    if (std::filesystem::exists(projInc + "/cpp-repl/fix_np_headers.hpp", ec)) {
      actualInc = projInc;
    } else if (std::filesystem::exists("include/cpp-repl/fix_np_headers.hpp", ec)) {
      actualInc = "include";
    } else if (std::filesystem::exists("/usr/include/cpp-repl/fix_np_headers.hpp", ec)) {
      actualInc = "/usr/include";
    } else if (std::filesystem::exists("/usr/local/include/cpp-repl/fix_np_headers.hpp", ec)) {
      actualInc = "/usr/local/include";
    } else {
      actualInc = "";
    }
    if (!actualInc.empty()) {
      compilerArgsStorage_.push_back("-I");
      compilerArgsStorage_.push_back(actualInc);
      compilerArgsStorage_.push_back("-include");
      compilerArgsStorage_.push_back("cpp-repl/fix_np_headers.hpp");
    } else {
      // No fix header found (e.g., running artefact outside source tree) – skip -include.
      // Still ensure a generic include dir for user code if available.
      if (std::filesystem::exists(projInc, ec)) {
        compilerArgsStorage_.push_back("-I");
        compilerArgsStorage_.push_back(projInc);
      } else if (std::filesystem::exists("include", ec)) {
        compilerArgsStorage_.push_back("-I");
        compilerArgsStorage_.push_back("include");
      } else if (std::filesystem::exists("/usr/include", ec)) {
        compilerArgsStorage_.push_back("-I");
        compilerArgsStorage_.push_back("/usr/include");
      }
    }
  }
  for (auto &p : includePaths_) {
    compilerArgsStorage_.push_back("-I");
    compilerArgsStorage_.push_back(p);
  }
  for (auto &d : defines_) {
    compilerArgsStorage_.push_back("-D");
    compilerArgsStorage_.push_back(d);
  }
  for (auto &p : libraryPaths_) {
    compilerArgsStorage_.push_back("-L");
    compilerArgsStorage_.push_back(p);
  }
  // Note: -l is handled via LoadDynamicLibrary, not compiler arg, but also add for completeness
  for (auto &l : libraries_) {
    // Keep as -l<lib> without space to match clang driver
    compilerArgsStorage_.push_back("-l" + l);
  }

  std::vector<const char *> args;
  args.reserve(compilerArgsStorage_.size());
  for (auto &s : compilerArgsStorage_) args.push_back(s.c_str());

  builder.SetCompilerArgs(args);
  auto CI = builder.CreateCpp();
  if (!CI) {
    llvm::handleAllErrors(
        CI.takeError(), [&](llvm::ErrorInfoBase &EIB) { err = EIB.message(); });
    return false;
  }
  auto interpOrErr = clang::Interpreter::create(std::move(*CI));
  if (!interpOrErr) {
    llvm::handleAllErrors(
        interpOrErr.takeError(),
        [&](llvm::ErrorInfoBase &EIB) { err = EIB.message(); });
    return false;
  }
  interp_ = std::move(*interpOrErr);
  initialized_ = true;

  // Inject BigInt support preamble
  if (utils::BigIntSupport::isAvailable()) {
    clang::Value V;
    auto e = interp_->ParseAndExecute(utils::BigIntSupport::preamble(), &V);
    if (e)
      llvm::handleAllErrors(std::move(e), [&](llvm::ErrorInfoBase &EIB) {});
#ifdef HAS_GMP
    auto e2 = interp_->ParseAndExecute(utils::BigIntSupport::gmpPreamble(), &V);
    if (e2)
      llvm::handleAllErrors(std::move(e2), [&](llvm::ErrorInfoBase &EIB) {});
#endif
  }
  // Auto-include standard library (bits/stdc++.h) by default for STL support
  tryIncludeStdLib();
  // Load libraries requested via -l / --library (absolute, relative, or -l name)
  for (auto &lib : libraries_) {
    auto tryLoad = [&](const std::string &path) -> bool {
      if (auto e = interp_->LoadDynamicLibrary(path.c_str())) {
        llvm::handleAllErrors(std::move(e), [&](llvm::ErrorInfoBase &EIB){});
        return false;
      }
      return true;
    };
    // If lib contains '/', treat as path (absolute or relative)
    if (lib.find('/') != std::string::npos) {
      if (tryLoad(lib)) continue;
      // Try with .so suffix if no extension
      if (lib.find(".so") == std::string::npos) {
        if (tryLoad(lib + ".so")) continue;
      }
      // Fall through to error (will be reported via LoadDynamicLibrary)
      continue;
    }
    // Bare lib name like "m", "gmp", "mylib"
    // Try as given, then lib + .so, then lib<name>.so variants
    if (tryLoad(lib)) continue;
    if (lib.find(".so") == std::string::npos) {
      if (tryLoad(lib + ".so")) continue;
      if (tryLoad("lib" + lib + ".so")) continue;
    }
    // Search in libraryPaths
    bool found = false;
    for (auto &lp : libraryPaths_) {
      std::string base = lp;
      if (!base.empty() && base.back() == '/') base.pop_back();
      if (tryLoad(base + "/" + lib)) { found = true; break; }
      if (lib.find(".so") == std::string::npos) {
        if (tryLoad(base + "/" + lib + ".so")) { found = true; break; }
        if (tryLoad(base + "/lib" + lib + ".so")) { found = true; break; }
      }
    }
    if (found) continue;
    // Also try system default search for -l<lib> style (e.g., -l m -> libm.so)
    if (lib.find(".so") == std::string::npos && lib.find("lib") != 0) {
      std::string sysLib = "lib" + lib + ".so";
      if (tryLoad(sysLib)) continue;
      // Try common system paths
      if (tryLoad("/usr/lib/" + sysLib)) continue;
      if (tryLoad("/usr/lib/x86_64-linux-gnu/" + sysLib)) continue;
    }
    // If still not found, keep library as is (error will be reported on use)
  }
  return true;
}

bool Interpreter::reinitWithCurrentOptions(std::string &err) {
  std::vector<std::string> oldHistory = history_;
  std::string local;
  interp_.reset();
  initialized_ = false;
  history_.clear();
  variables_.clear();
  tracker_->clear();
  varHistory_.clear();
  stdLibIncluded_ = false;
  if (!init(currentVersion_, includePaths_, defines_, libraryPaths_, libraries_, local)) {
    err = local;
    return false;
  }
  for (auto &h : oldHistory) {
    std::string e;
    if (!eval(h, e)) { /* keep going */ }
  }
  return true;
}

auto Interpreter::addIncludePath(const std::string &path, std::string &err) -> bool {
  for (auto &p : includePaths_) if (p == path) return true;
  includePaths_.push_back(path);
  return reinitWithCurrentOptions(err);
}
auto Interpreter::addLibraryPath(const std::string &path, std::string &err) -> bool {
  for (auto &p : libraryPaths_) if (p == path) return true;
  libraryPaths_.push_back(path);
  return reinitWithCurrentOptions(err);
}
auto Interpreter::addLibrary(const std::string &lib, std::string &err) -> bool {
  libraries_.push_back(lib);
  if (!initialized_) return true;
  auto tryLoad = [&](const std::string &path) -> bool {
    // std::cerr << "[tryLoad " << path << "]\n";
    if (auto e = interp_->LoadDynamicLibrary(path.c_str())) {
      llvm::handleAllErrors(std::move(e), [&](llvm::ErrorInfoBase &EIB){ err = EIB.message(); });
      return false;
    }
    err.clear();
    return true;
  };
  if (lib.find('/') != std::string::npos) {
    if (tryLoad(lib)) return true;
    if (lib.find(".so") == std::string::npos) {
      if (tryLoad(lib + ".so")) return true;
    }
    return false;
  }
  if (tryLoad(lib)) return true;
  if (lib.find(".so") == std::string::npos) {
    if (tryLoad(lib + ".so")) return true;
    if (tryLoad("lib" + lib + ".so")) return true;
  }
  for (auto &lp : libraryPaths_) {
    std::string base = lp;
    if (!base.empty() && base.back() == '/') base.pop_back();
    if (tryLoad(base + "/" + lib)) return true;
    if (lib.find(".so") == std::string::npos) {
      if (tryLoad(base + "/" + lib + ".so")) return true;
      if (tryLoad(base + "/lib" + lib + ".so")) return true;
    }
  }
  if (lib.find(".so") == std::string::npos && lib.find("lib") != 0) {
    std::string sysLib = "lib" + lib + ".so";
    if (tryLoad(sysLib)) return true;
    if (tryLoad("/usr/lib/" + sysLib)) return true;
    if (tryLoad("/usr/lib/x86_64-linux-gnu/" + sysLib)) return true;
  }
  return false;
}

bool Interpreter::ensureVersion(utils::StdVersion needed, std::string &err) {
  if (!initialized_)
    return init(needed, includePaths_, defines_, libraryPaths_, libraries_, err);
  if (static_cast<int>(needed) <= static_cast<int>(currentVersion_))
    return true;
  // Need higher version – re-init and replay history, preserving include/lib
  std::vector<std::string> oldHistory = history_;
  std::string local;
  interp_.reset();
  initialized_ = false;
  history_.clear();
  variables_.clear();
  tracker_->clear();
  varHistory_.clear();
  stdLibIncluded_ = false;
  if (!init(needed, includePaths_, defines_, libraryPaths_, libraries_, local)) {
    err = local;
    return false;
  }
  for (auto &h : oldHistory) {
    std::string e;
    if (!eval(h, e)) {
      // if replay fails, keep going
    }
  }
  return true;
}

bool Interpreter::tryIncludeStdLib() {
  if (stdLibIncluded_ || !initialized_ || !interp_) return stdLibIncluded_;
  clang::Value V;
  // Try bits/stdc++.h first (covers everything)
  auto e = interp_->ParseAndExecute("#include <bits/stdc++.h>\n", &V);
  if (!e) { stdLibIncluded_ = true; return true; }
  llvm::handleAllErrors(std::move(e), [&](llvm::ErrorInfoBase &EIB){});
  // Fallback: include common STL headers individually
  const char *fallback = R"(
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <utility>
#include <functional>
#include <numeric>
#include <iterator>
#include <memory>
#include <type_traits>
#include <limits>
#include <cstdint>
#include <cstddef>
)";
  auto e2 = interp_->ParseAndExecute(fallback, &V);
  if (!e2) { stdLibIncluded_ = true; return true; }
  llvm::handleAllErrors(std::move(e2), [&](llvm::ErrorInfoBase &EIB){});
  return false;
}

bool Interpreter::ensureStdLib(std::string &err) {
  if (stdLibIncluded_) return true;
  if (!initialized_) { err = "REPL not initialized"; return false; }
  if (tryIncludeStdLib()) { err.clear(); return true; }
  err = "failed to include standard library (bits/stdc++.h)";
  return false;
}

bool Interpreter::evalAuto(const std::string &code, std::string &err) {
  auto needed = utils::VersionDetector::detect(code);
  if (!ensureVersion(needed, err))
    return false;
  return eval(code, err);
}

static std::string preprocessBigIntLiterals(const std::string &code) {
  // Handle: bigint g = 4949... (50 digits) -> bigint g = bigint("4949...")
  // Also cpp_int, mpz_int, mpz, mpq
  // Any integer literal with 19+ digits (exceeds int64) assigned to bigint-like type
  // should be wrapped as string constructor to avoid "integer literal is too large"
  std::string result = code;
  // Regex for (bigint|cpp_int|mpz_int|mpz|mpq)\s+(\w+)\s*=\s*([0-9]{19,})\s*;
  // Use ECMA regex and replace
    // NOTE: regex may throw std::regex_error; with -fno-exceptions we avoid try/catch
    // and assume pattern is valid (tested). If it throws, it will terminate.
    std::regex re(R"((\b(?:bigint|cpp_int|mpz_int|mpz|mpq_rational|mpq)\b\s+\w+\s*=\s*)([0-9]{19,})(\s*;))");
    // Wrap the digits in type("digits")
    // We need to know the type to wrap correctly: e.g., bigint g = 123 -> bigint g = bigint("123");
    // So we capture prefix and digits and suffix
    std::string out;
    std::sregex_iterator it(result.begin(), result.end(), re);
    std::sregex_iterator end;
    size_t lastPos = 0;
    for (; it != end; ++it) {
      auto &m = *it;
      std::string prefix = m[1].str();
      std::string digits = m[2].str();
      std::string suffix = m[3].str();
      // Extract type from prefix (first word)
      std::string type;
      {
        std::istringstream iss(prefix);
        iss >> type;
        // type may be like "bigint" or "const bigint" – find last word before variable
        // For simplicity, find first occurrence of bigint etc. in prefix
        if (prefix.find("bigint") != std::string::npos) type = "bigint";
        else if (prefix.find("cpp_int") != std::string::npos) type = "cpp_int";
        else if (prefix.find("mpz_int") != std::string::npos) type = "mpz_int";
        else if (prefix.find("mpz") != std::string::npos) type = "mpz";
        else if (prefix.find("mpq") != std::string::npos) type = "mpq_rational";
        else type = "bigint";
      }
      out.append(result, lastPos, m.position() - lastPos);
      out += prefix + type + "(\"" + digits + "\")" + suffix;
      lastPos = m.position() + m.length();
    }
    out.append(result, lastPos, std::string::npos);
    if (lastPos != 0) result = out;

    // Also handle auto g = 4949... where 4949... is large and initializing bigint-like auto
    // For auto with large literal, wrap as cpp_int("...")
    std::regex autoRe(R"((\bauto\b\s+\w+\s*=\s*)([0-9]{19,})(\s*;))");
    out.clear();
    lastPos = 0;
    std::sregex_iterator it2(result.begin(), result.end(), autoRe);
    for (; it2 != end; ++it2) {
      auto &m = *it2;
      std::string prefix = m[1].str();
      std::string digits = m[2].str();
      std::string suffix = m[3].str();
      out.append(result, lastPos, m.position() - lastPos);
      out += prefix + "cpp_int(\"" + digits + "\")" + suffix;
      lastPos = m.position() + m.length();
    }
    if (lastPos != 0) {
      out.append(result, lastPos, std::string::npos);
      result = out;
    }
  return result;
}

// --- copied eval logic from legacy Repl (interpreter-like) ---
static inline std::string trim_copy(const std::string &s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos)
    return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}
static inline std::string rtrim_semi(const std::string &s) {
  std::string t = trim_copy(s);
  while (!t.empty() &&
         (t.back() == ';' || t.back() == '\n' || t.back() == '\r' ||
          t.back() == ' ' || t.back() == '\t')) {
    if (t.back() == ';') {
      t.pop_back();
      t = trim_copy(t);
      break;
    }
    t.pop_back();
  }
  return t;
}

std::string Interpreter::normalizeValue(const std::string &v) {
  std::string t = trim_copy(v);
  if (!t.empty() && t.back() == ';') {
    t.pop_back();
    t = trim_copy(t);
  }
  std::string out;
  bool inSpace = false;
  for (char c : t) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      if (!inSpace) out.push_back(' ');
      inSpace = true;
    } else {
      out.push_back(c);
      inSpace = false;
    }
  }
  return trim_copy(out);
}

bool Interpreter::parseDeclaration(const std::string &code, std::string &type,
                                   std::string &name, std::string &value) {
  std::string t = trim_copy(code);
  static std::regex declRegex(
      R"(^\s*((?:(?:const|constexpr|static|volatile|inline|extern|mutable)\s+)*)([\w:\<\>\,\s]+?)\s*([\*\&]*)\s*(\w+)\s*(?:=\s*(.+?)|\s*(\(.+?\)|\{.+?\}))?\s*;?\s*$)",
      std::regex::ECMAScript);
  std::smatch m;
  if (!std::regex_match(t, m, declRegex)) return false;
  std::string qualifiers = trim_copy(m[1].str());
  std::string rawType = trim_copy(m[2].str());
  std::string stars = trim_copy(m[3].str());
  std::string rawName = trim_copy(m[4].str());
  std::string rawVal;
  if (m[5].matched) rawVal = trim_copy(m[5].str());
  else if (m[6].matched) rawVal = trim_copy(m[6].str());
  else rawVal = std::string();
  if (rawType.empty()) return false;
  if (rawName == "if" || rawName == "for" || rawName == "while" || rawName == "return")
    return false;
  std::string fullType = trim_copy(qualifiers + (qualifiers.empty() ? "" : " ") + rawType);
  if (!stars.empty()) {
    fullType = trim_copy(fullType + " " + stars);
  }
  fullType = normalizeValue(fullType);
  std::string lower = fullType;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  bool hasTypeKeyword = lower.find("int") != std::string::npos ||
                        lower.find("double") != std::string::npos ||
                        lower.find("float") != std::string::npos ||
                        lower.find("char") != std::string::npos ||
                        lower.find("long") != std::string::npos ||
                        lower.find("short") != std::string::npos ||
                        lower.find("unsigned") != std::string::npos ||
                        lower.find("auto") != std::string::npos ||
                        lower.find("string") != std::string::npos ||
                        lower.find("bool") != std::string::npos ||
                        lower.find("cpp_int") != std::string::npos ||
                        lower.find("bigint") != std::string::npos ||
                        lower.find("vector") != std::string::npos ||
                        lower.find("map") != std::string::npos ||
                        lower.find("std::") != std::string::npos ||
                        fullType.find("::") != std::string::npos ||
                        fullType.find("<") != std::string::npos ||
                        fullType.find("*") != std::string::npos ||
                        fullType.find("&") != std::string::npos;
  if (!hasTypeKeyword) return false;
  type = fullType;
  name = rawName;
  value = normalizeValue(rawVal);
  return true;
}

bool Interpreter::parseAssignment(const std::string &code, std::string &name,
                                  std::string &value) {
  std::string t = trim_copy(code);
  static std::regex assignRegex(R"(^\s*(\w+)\s*=\s*(.+?)\s*;?\s*$)");
  std::smatch m;
  if (!std::regex_match(t, m, assignRegex)) return false;
  name = trim_copy(m[1].str());
  value = normalizeValue(trim_copy(m[2].str()));
  return true;
}

std::string Interpreter::sanitizeIncludes(const std::string &code) {
  std::istringstream iss(code);
  std::string line;
  std::string result;
  bool changed = false;
  while (std::getline(iss, line)) {
    std::string trimmed = trim_copy(line);
    std::string resultLine = line;
    if (trimmed.rfind("#include", 0) == 0) {
      std::string t = trim_copy(line);
      if (!t.empty() && t.back() == ';') {
        size_t pos = line.find_last_of(';');
        if (pos != std::string::npos) {
          resultLine = line.substr(0, pos);
          changed = true;
        }
      }
      size_t q1 = resultLine.find('"');
      if (q1 != std::string::npos) {
        size_t q2 = resultLine.find('"', q1 + 1);
        if (q2 != std::string::npos) {
          std::string incPath = resultLine.substr(q1 + 1, q2 - q1 - 1);
          std::error_code ec;
          bool exists = std::filesystem::exists(incPath, ec);
          bool isDir = !ec && std::filesystem::is_directory(incPath, ec);
          if (!ec && exists && isDir) {}
        }
      }
    }
    result += resultLine + "\n";
  }
  if (!changed) return code;
  return result;
}

bool Interpreter::checkVariableRedefinition(const std::string &code, std::string &err) {
  std::string trimmed = trim_copy(code);
  if (trimmed.find('\n') != std::string::npos) return true;
  if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ':') return true;
  if (trimmed.rfind("int ", 0) == 0 && trimmed.find('(') != std::string::npos && trimmed.find(')') != std::string::npos && trimmed.find('{') != std::string::npos) return true;
  if (trimmed.find("for") == 0 || trimmed.find("while") == 0 || trimmed.find("if") == 0 || trimmed.find("struct ") != std::string::npos || trimmed.find("class ") != std::string::npos)
    return true;
  std::string type, name, value;
  if (parseDeclaration(trimmed, type, name, value)) {
    utils::VarInfo info{type, value};
    auto prev = tracker_->find(name);
    if (prev) {
      if (prev->type == type && prev->value == value) {
        std::cout << "[ignored: redefinition of '" << name << "' with same value " << value << " (type " << type << ")]\n";
        return false;
      } else {
        err = "redefinition of '" + name + "' with different value (previous: " + prev->value + " [" + prev->type + "] vs new: " + value + " [" + type + "])";
        err += " [hint: same name & same value is allowed and ignored]";
        return false;
      }
    }
  }
  return true;
}

void Interpreter::trackVariable(const std::string &code) {
  std::string trimmed = trim_copy(code);
  if (trimmed.find('\n') != std::string::npos) return;
  if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ':') return;
  std::string type, name, value;
  if (parseDeclaration(trimmed, type, name, value)) {
    varHistory_.push_back(variables_);
    variables_[name] = {type, value};
    tracker_->track(name, {type, value});
    return;
  }
  std::string aName, aVal;
  if (parseAssignment(trimmed, aName, aVal)) {
    auto it = variables_.find(aName);
    if (it != variables_.end()) {
      varHistory_.push_back(variables_);
      it->second.second = aVal;
      tracker_->track(aName, {it->second.first, aVal});
    }
  }
}

bool Interpreter::eval(const std::string &code, std::string &err) {
  std::string preprocessed = preprocessBigIntLiterals(code);
  std::string sanitized = sanitizeIncludes(preprocessed);
  auto needed = utils::VersionDetector::detect(sanitized);
  if (static_cast<int>(needed) > static_cast<int>(currentVersion_)) {
    std::string vErr;
    if (!ensureVersion(needed, vErr)) {
      err = vErr;
      return false;
    }
  }

  if (!initialized_ || !interp_) {
    err = "REPL not initialized";
    return false;
  }
  std::string trimmed = trim_copy(sanitized);
  if (trimmed.empty())
    return true;

  // Proactive stdlib: include <bits/stdc++.h> on first use of std::
  if (!stdLibIncluded_ && sanitized.find("std::") != std::string::npos) {
    std::string dummy;
    ensureStdLib(dummy);
  }

  // Security hardening: check code for dangerous patterns (system, popen, etc.)
  {
    std::string secErr = sandbox_.check(sanitized);
    if (!secErr.empty()) {
      err = secErr;
      return false;
    }
  }

  if (trimmed.rfind("#include", 0) == 0) {
    size_t q1 = sanitized.find('"');
    size_t q2 = std::string::npos;
    if (q1 != std::string::npos) q2 = sanitized.find('"', q1 + 1);
    std::string incPath;
    if (q1 != std::string::npos && q2 != std::string::npos) incPath = sanitized.substr(q1 + 1, q2 - q1 - 1);
    else {
      size_t a1 = sanitized.find('<');
      size_t a2 = sanitized.find('>', a1 + 1);
      if (a1 != std::string::npos && a2 != std::string::npos) incPath = sanitized.substr(a1 + 1, a2 - a1 - 1);
    }
    if (!incPath.empty()) {
      std::error_code ec;
      bool exists = std::filesystem::exists(incPath, ec);
      bool isDir = !ec && std::filesystem::is_directory(incPath, ec);
      if (!ec && exists && isDir) {
        err = "fatal error: '" + incPath + "' is a directory, not a file [hint] Did you mean '" + incPath + "/np.hpp'? Use -I <path-to-Numpy-C-API>/include and #include \"np/np.hpp\" or #include <np/np.hpp>. Available: ";
        std::error_code ec2;
        int cnt = 0;
        for (auto &entry : std::filesystem::directory_iterator(incPath, ec2)) {
          if (ec2) break;
          if (cnt++ >= 6) { err += "..."; break; }
          err += entry.path().filename().string() + " ";
        }
        return false;
      }
    }
  }

  {
    std::string redefErr;
    if (!checkVariableRedefinition(sanitized, redefErr)) {
      if (!redefErr.empty()) {
        err = redefErr;
        return false;
      } else {
        return true;
      }
    }
  }

  std::string toEval = sanitized;
  bool needsSemi = false;
  if (!trimmed.empty() && trimmed.back() != ';' && trimmed[0] != '#') {
    // Don't auto-add ; for plain blocks that are not variable declarations
    bool isBraceBlock = (trimmed.back() == '}' || trimmed.back() == '{') && trimmed.find('=') == std::string::npos;
    if (!isBraceBlock) {
      bool isDecl =
          trimmed.find('=') != std::string::npos ||
          trimmed.rfind("int ", 0) == 0 || trimmed.rfind("auto ", 0) == 0 ||
          trimmed.rfind("float ", 0) == 0 || trimmed.rfind("double ", 0) == 0 ||
          trimmed.rfind("char ", 0) == 0 || trimmed.rfind("std::", 0) == 0 ||
          trimmed.rfind("const ", 0) == 0 || trimmed.rfind("string ", 0) == 0 ||
          trimmed.rfind("long ", 0) == 0 || trimmed.rfind("unsigned ", 0) == 0 ||
          trimmed.find('*') != std::string::npos || trimmed.find("FILE") != std::string::npos;
      if (!isDecl) {
        std::string tmpT, tmpN, tmpV;
        if (parseDeclaration(trimmed + ";", tmpT, tmpN, tmpV)) isDecl = true;
      }
      if (isDecl) {
        toEval = trimmed + ";\n";
        needsSemi = true;
      }
    }
  }

  clang::Value V;
  auto e = interp_->ParseAndExecute(toEval, &V);
  if (e) {
    std::string msg;
    llvm::handleAllErrors(
        std::move(e), [&](llvm::ErrorInfoBase &EIB) { msg = EIB.message(); });
    if (needsSemi) {
      clang::Value V2;
      auto e2 = interp_->ParseAndExecute(sanitized, &V2);
      if (!e2) {
        if (V2.isValid()) {
          highPrecisionDump(V2);
          std::cout << "\n";
        }
        if (!sanitized.empty()) {
          history_.push_back(sanitized);
          trackVariable(sanitized);
        }
        return true;
      }
      llvm::handleAllErrors(std::move(e2), [&](llvm::ErrorInfoBase &EIB) {});
    }
    if (trimmed[0] != '#' && !needsSemi && !trimmed.empty() && trimmed.back() != ';' &&
        trimmed.back() != '}' && trimmed.back() != '{') {
      std::string withSemi = trimmed + ";\n";
      clang::Value V2;
      auto e2 = interp_->ParseAndExecute(withSemi, &V2);
      if (!e2) {
        if (V2.isValid()) {
          highPrecisionDump(V2);
          std::cout << "\n";
        }
        if (!sanitized.empty()) {
          history_.push_back(sanitized);
          trackVariable(sanitized);
        }
        return true;
      }
      llvm::handleAllErrors(std::move(e2), [&](llvm::ErrorInfoBase &EIB) {});
    }
    // Auto-upgrade C++ standard if error indicates need for C++20/23 (e.g. header requires it)
    {
      utils::StdVersion higher = utils::StdVersion::Cpp17;
      if (msg.find("source_location") != std::string::npos ||
          msg.find("std::format") != std::string::npos ||
          msg.find("concept") != std::string::npos ||
          msg.find("requires") != std::string::npos ||
          msg.find("co_await") != std::string::npos ||
          msg.find("char8_t") != std::string::npos ||
          msg.find("consteval") != std::string::npos ||
          msg.find("only available with '-std=c++20'") != std::string::npos ||
          msg.find("is only available from C++20") != std::string::npos) {
        higher = utils::StdVersion::Cpp20;
      }
      if (msg.find("import") != std::string::npos && msg.find("module") != std::string::npos) {
        higher = utils::StdVersion::Cpp23;
      }
      if (static_cast<int>(higher) > static_cast<int>(currentVersion_)) {
        std::string vErr;
        if (ensureVersion(higher, vErr)) {
          clang::Value V2;
          auto e2 = interp_->ParseAndExecute(toEval, &V2);
          if (!e2) {
            if (V2.isValid()) {
              bool shouldPrint2 = true;
              if (V2.isVoid()) shouldPrint2 = false;
              else if (trimmed.find("std::cout") != std::string::npos) shouldPrint2 = false;
              if (shouldPrint2) { highPrecisionDump(V2); std::cout << "\n"; }
            }
            if (!sanitized.empty()) {
              history_.push_back(sanitized);
              trackVariable(sanitized);
            }
            std::cout << "[auto-upgraded to " << utils::VersionDetector::toString(higher) << " for header compatibility]\n";
            return true;
          }
          llvm::handleAllErrors(std::move(e2), [&](llvm::ErrorInfoBase &EIB){});
        }
      }
    }
    if (msg.find("file not found") != std::string::npos) {
      if (msg.find("/np'") != std::string::npos || msg.find("/np\"") != std::string::npos) {
        msg += "\n[hint] Did you mean \"/.../include/np/np.hpp\"? Use -I <path-to-Numpy-C-API>/include and #include \"np/np.hpp\" or #include <np/np.hpp>";
      }
      if (sanitized.find("#include") != std::string::npos) {
        size_t q1 = sanitized.find('"');
        size_t q2 = sanitized.find('"', q1 + 1);
        if (q1 != std::string::npos && q2 != std::string::npos) {
          std::string incPath = sanitized.substr(q1 + 1, q2 - q1 - 1);
          std::error_code ec;
          bool exists = std::filesystem::exists(incPath, ec);
          bool isDir = !ec && std::filesystem::is_directory(incPath, ec);
          if (!ec && exists && isDir) {
            msg += "\n[hint] '" + incPath + "' is a directory, not a file. Try including a specific header like '" + incPath + "/np.hpp' or use -I with <np/...>";
            msg += "\n[hint] Available headers in " + incPath + ": ";
            int cnt = 0;
            std::error_code ec2;
            for (auto &entry : std::filesystem::directory_iterator(incPath, ec2)) {
              if (ec2) break;
              if (cnt++ >= 5) { msg += "..."; break; }
              msg += entry.path().filename().string() + " ";
            }
          }
        }
      }
    }
    if (msg.find("redefinition") != std::string::npos) {
      std::string type, name, value;
      if (parseDeclaration(sanitized, type, name, value)) {
        auto it = variables_.find(name);
        if (it != variables_.end()) {
          if (it->second.first == type && it->second.second == value) {
            std::cout << "[ignored: redefinition of '" << name << "' with same value " << value << "]\n";
            return true;
          } else {
            err = "redefinition of '" + name + "' with different value (previous: " + it->second.second + " [" + it->second.first + "] vs new: " + value + " [" + type + "])";
            err += " [hint: same name & same value is allowed and ignored]";
            return false;
          }
        }
      }
    }
    // Handle Numpy-C-API ProxyBase issues without modifying headers (C++23)
    if (msg.find("ProxyBase") != std::string::npos || msg.find("convert_to") != std::string::npos ||
        msg.find("fixed_source") != std::string::npos || msg.find("vector<bool>") != std::string::npos) {
      msg += "\n[hint] Numpy-C-API ProxyBase/vector<bool> issue – header uses C++23 and boost::multiprecision. "
             "Try: cpp-repl -std=c++23 -I <path-to-Numpy-C-API>/include "
             "or use static_cast<np::bigint>(proxy).convert_to<double>() and "
             "static_cast<np::bigint>(a[n]) for ap*a[n]";
    }
    // Auto-include standard library if keyword suggests missing header
    // e.g., std::exp, std::forward, std::vector without prior include
    if (!stdLibIncluded_ && sanitized.find("std::") != std::string::npos &&
        (msg.find("undeclared identifier") != std::string::npos ||
         msg.find("no member named") != std::string::npos ||
         msg.find("has no member") != std::string::npos ||
         msg.find("unknown type name") != std::string::npos ||
         msg.find("use of undeclared") != std::string::npos ||
         msg.find("implicit instantiation") != std::string::npos)) {
      std::string dummy;
      if (ensureStdLib(dummy)) {
        clang::Value V2;
        auto e2 = interp_->ParseAndExecute(toEval, &V2);
        if (!e2) {
          if (V2.isValid()) {
            bool shouldPrint = true;
            if (V2.isVoid()) shouldPrint = false;
            else if (trimmed.find("std::cout") != std::string::npos) shouldPrint = false;
            if (shouldPrint) { highPrecisionDump(V2); std::cout << "\n"; }
          }
          if (!sanitized.empty()) {
            history_.push_back(sanitized);
            trackVariable(sanitized);
          }
          std::cout << "[auto-included <bits/stdc++.h> for std:: support]\n";
          return true;
        }
        llvm::handleAllErrors(std::move(e2), [&](llvm::ErrorInfoBase &EIB){ msg = EIB.message(); });
        msg += "\n[hint] tried auto-including <bits/stdc++.h> but still failed; try explicit #include <...> or check std:: usage";
      }
    }
    // JIT poison recovery: Symbols not found / Failed to materialize symbols
    if (msg.find("Symbols not found") != std::string::npos ||
        msg.find("Failed to materialize symbols") != std::string::npos ||
        msg.find("__orc_init_func") != std::string::npos) {
      // Attempt to undo the poisoned increment; clang::Interpreter::Undo(1) often clears it
      if (auto ue = interp_->Undo(1)) {
        llvm::handleAllErrors(std::move(ue), [&](llvm::ErrorInfoBase &EIB){});
      }
      // Also pop last history if it was the poisoned one (if any)
      // No push yet for this failed eval, so nothing to pop from history, but prior poisoned def may be in history
      // Offer hint and suggest :reset if still broken
      if (msg.find("Failed to materialize") != std::string::npos) {
        msg += "\n[hint] JIT poisoned — auto-undo attempted. If subsequent #include still fails, run :reset or restart. Original error was likely a bad template (e.g., std::forward without <T>).";
      } else {
        msg += "\n[hint] Symbols not found — likely a failed template instantiation (e.g., std::forward(x) should be std::forward<T>(x)). Auto-undo attempted; try :undo or :reset.";
      }
    }
    err = msg;
    return false;
  }
  if (V.isValid()) {
    bool shouldPrint = true;
    if (V.isVoid())
      shouldPrint = false;
    else if (trimmed.find("std::cout") != std::string::npos ||
             trimmed.find("printf") != std::string::npos)
      shouldPrint = false;
    else {
      // BigInt support – like Python, print actual value via std::cout for
      // cpp_int/mpz
      std::string typeStr;
      llvm::raw_string_ostream os(typeStr);
      V.printType(os);
      os.flush();
      if (typeStr.find("cpp_int") != std::string::npos ||
          typeStr.find("mpz_int") != std::string::npos ||
          typeStr.find("mpq_rational") != std::string::npos ||
          typeStr.find("cpp_dec_float") != std::string::npos ||
          typeStr.find("boost::multiprecision") != std::string::npos) {
        // Check if expr is simple and not already a cout
        std::string expr = rtrim_semi(trimmed);
        // Avoid double cout for already cout expressions
        if (expr.find("std::cout") == std::string::npos) {
          std::string printCode = "std::cout << (" + expr + ") << std::endl;";
          clang::Value dummy;
          auto e2 = interp_->ParseAndExecute(printCode, &dummy);
          if (e2)
            llvm::handleAllErrors(std::move(e2),
                                  [&](llvm::ErrorInfoBase &EIB) {});
          shouldPrint = false; // already printed via cout
        }
      }
    }
    if (shouldPrint) {
      highPrecisionDump(V);
      std::cout << "\n";
    }
  } else {
    std::string t = trim_copy(sanitized);
    if (!t.empty() && t.back() == ';') {
      bool likelyExpr = true;
      if (t.find("int ") != std::string::npos ||
          t.find("auto ") != std::string::npos ||
          t.find("double ") != std::string::npos ||
          t.find("float ") != std::string::npos ||
          t.find("char ") != std::string::npos ||
          t.find("long ") != std::string::npos ||
          t.find("unsigned ") != std::string::npos ||
          t.find("const ") != std::string::npos ||
          t.find("std::") != std::string::npos ||
          t.find("bool ") != std::string::npos ||
          t.find("=") != std::string::npos ||
          t.find("#include") != std::string::npos || t.find("for") == 0 ||
          t.find("while") == 0 || t.find("if") == 0 ||
          t.find("struct ") != std::string::npos ||
          t.find("class ") != std::string::npos ||
          t.find("using ") != std::string::npos ||
          t.find("std::cout") != std::string::npos ||
          t.find("printf") != std::string::npos)
        likelyExpr = false;
      std::string stripped = rtrim_semi(sanitized);
      if (likelyExpr && stripped.find(';') == std::string::npos &&
          stripped.size() < 200) {
        clang::Value V2;
        auto e2 = interp_->ParseAndExecute(stripped, &V2);
        if (!e2 && V2.isValid()) {
          // BigInt check for stripped expr as well
          std::string typeStr2;
          llvm::raw_string_ostream os2(typeStr2);
          V2.printType(os2);
          os2.flush();
          if (typeStr2.find("cpp_int") != std::string::npos ||
              typeStr2.find("mpz_int") != std::string::npos ||
              typeStr2.find("boost::multiprecision") != std::string::npos) {
            std::string printCode =
                "std::cout << (" + stripped + ") << std::endl;";
            clang::Value dummy;
            auto e3 = interp_->ParseAndExecute(printCode, &dummy);
            if (e3)
              llvm::handleAllErrors(std::move(e3),
                                    [&](llvm::ErrorInfoBase &EIB) {});
          } else {
            highPrecisionDump(V2);
            std::cout << "\n";
          }
        } else if (e2)
          llvm::handleAllErrors(std::move(e2),
                                [&](llvm::ErrorInfoBase &EIB) {});
      }
    }
  }
  if (!sanitized.empty()) {
    history_.push_back(sanitized);
    trackVariable(sanitized);
  }
  return true;
}

bool Interpreter::eval(const std::string &code, std::string &err,
                       bool &incomplete) {
  incomplete = utils::IncompleteDetector::isIncomplete(code);
  if (incomplete) return true;
  return eval(code, err);
}

bool Interpreter::loadFile(const std::string &path, std::string &err) {
  std::ifstream f(path);
  if (!f) {
    err = "cannot open file: " + path;
    return false;
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  return eval(ss.str(), err);
}
bool Interpreter::loadLibrary(const std::string &path, std::string &err) {
  if (!initialized_ || !interp_) {
    err = "REPL not initialized";
    return false;
  }
  if (auto e = interp_->LoadDynamicLibrary(path.c_str())) {
    llvm::handleAllErrors(
        std::move(e), [&](llvm::ErrorInfoBase &EIB) { err = EIB.message(); });
    return false;
  }
  return true;
}
bool Interpreter::undo(unsigned n, std::string &err) {
  if (!initialized_ || !interp_) {
    err = "REPL not initialized";
    return false;
  }
  if (auto e = interp_->Undo(n)) {
    llvm::handleAllErrors(
        std::move(e), [&](llvm::ErrorInfoBase &EIB) { err = EIB.message(); });
    return false;
  }
  while (n-- > 0 && !history_.empty())
    history_.pop_back();
  variables_.clear();
  tracker_->clear();
  for (auto &h : history_) {
    std::string type, name, value;
    if (parseDeclaration(h, type, name, value)) {
      variables_[name] = {type, value};
      tracker_->track(name, {type, value});
    } else {
      std::string aName, aVal;
      if (parseAssignment(h, aName, aVal)) {
        auto it = variables_.find(aName);
        if (it != variables_.end()) {
          it->second.second = aVal;
          tracker_->track(aName, {it->second.first, aVal});
        }
      }
    }
  }
  varHistory_.clear();
  return true;
}
void Interpreter::dump() const {
  std::cout << "=== REPL history (" << history_.size() << " inputs) ["
            << utils::VersionDetector::toString(currentVersion_) << "] ===\n";
  for (size_t i = 0; i < history_.size(); ++i) {
    std::cout << "[" << i << "] " << history_[i];
    if (history_[i].empty() || history_[i].back() != '\n')
      std::cout << "\n";
    std::cout << "---\n";
  }
  if (history_.empty())
    std::cout << "(no inputs yet)\n";
}
void Interpreter::stackLayout() const {
  bool useColor = isatty(STDOUT_FILENO) && !getenv("NO_COLOR") && !getenv("CPP_REPL_NO_COLOR");
  const char *term = getenv("TERM");
  if (useColor && term && std::string(term) == "dumb") useColor = false;
  auto col = [&](const char* c){ return useColor ? std::string(c) : std::string(""); };
  auto rst = col("\033[0m");
  auto cyan = col("\033[36m");
  auto grey = col("\033[90m");
  auto yellow = col("\033[33m");
  auto green = col("\033[32m");
  std::cout << cyan << "┌─[stack]─ Current Layout ─────────────────────" << rst << "\n";
  std::cout << grey << "│ " << rst << "Version: " << yellow << utils::VersionDetector::toString(currentVersion_) << rst << " (" << grey << utils::VersionDetector::toFlag(currentVersion_) << rst << ")\n";
  std::cout << grey << "│ " << rst << "History: " << green << history_.size() << rst << " inputs";
  if (!history_.empty()) std::cout << "  " << grey << "(last: " << history_.back().substr(0, 60) << (history_.back().size()>60?"...":"") << ")" << rst;
  std::cout << "\n";
  std::cout << grey << "│ " << rst << "Variables: " << green << variables_.size() << rst << " tracked";
  if (tracker_) std::cout << " (" << tracker_->size() << " via tracker)";
  std::cout << "\n";
  for (auto &kv : variables_) {
    std::cout << grey << "│   • " << rst << cyan << kv.second.first << rst << " " << yellow << kv.first << rst;
    if (!kv.second.second.empty()) std::cout << " = " << green << kv.second.second << rst;
    else std::cout << " " << grey << "[declared]" << rst;
    std::cout << "\n";
  }
  if (variables_.empty()) std::cout << grey << "│   (none)" << rst << "\n";
  std::cout << grey << "│ " << rst << "Includes: " << (includePaths_.empty() ? grey + "(none)" + rst : "");
  for (auto &p : includePaths_) std::cout << (includePaths_.size()?"\n"+grey+"│   • "+rst:"") << p;
  if (!includePaths_.empty()) std::cout << "\n";
  else std::cout << "\n";
  std::cout << grey << "│ " << rst << "Library paths: " << (libraryPaths_.empty() ? grey + "(none)" + rst : "");
  for (auto &p : libraryPaths_) std::cout << (libraryPaths_.size()?"\n"+grey+"│   • "+rst:"") << p;
  if (!libraryPaths_.empty()) std::cout << "\n";
  else std::cout << "\n";
  std::cout << grey << "│ " << rst << "Libraries: " << (libraries_.empty() ? grey + "(none)" + rst : "");
  for (auto &l : libraries_) std::cout << (libraries_.size()?"\n"+grey+"│   • "+rst:"") << l;
  if (!libraries_.empty()) std::cout << "\n";
  else std::cout << "\n";
  std::cout << grey << "│ " << rst << "Defines: " << (defines_.empty() ? grey + "(none)" + rst : "");
  for (auto &d : defines_) std::cout << (defines_.size()?"\n"+grey+"│   • "+rst:"") << d;
  if (!defines_.empty()) std::cout << "\n";
  else std::cout << "\n";
  std::cout << grey << "│ " << rst << "StdLib: " << (stdLibIncluded_ ? green + "included (bits/stdc++.h)" + rst : grey + "not yet included" + rst) << "\n";
  std::cout << grey << "│ " << rst << "BigInt: " << (utils::BigIntSupport::isAvailable() ? green + "yes (boost::multiprecision::cpp_int)" + rst : grey + "no" + rst) << "\n";
  std::cout << cyan << "└──────────────────────────────────────────────" << rst << "\n";
}
bool Interpreter::stackPop(unsigned n, std::string &err) {
  if (n == 0) n = 1;
  if (n > history_.size()) n = static_cast<unsigned>(history_.size());
  if (n == 0) { err = "stack empty"; return false; }
  return undo(n, err);
}
bool Interpreter::stackPush(const std::string &code, std::string &err) {
  return eval(code, err);
}
bool Interpreter::stackClear(std::string &err) {
  // Clear all history and variables but keep includes/defines/libraries and version
  std::string local;
  // Undo all
  if (!history_.empty()) {
    unsigned n = static_cast<unsigned>(history_.size());
    if (auto e = interp_->Undo(n)) {
      llvm::handleAllErrors(std::move(e), [&](llvm::ErrorInfoBase &EIB){ err = EIB.message(); });
      // Fallback to reset if undo fails
      reset(err);
      return err.empty();
    }
    history_.clear();
    variables_.clear();
    tracker_->clear();
    varHistory_.clear();
  } else {
    variables_.clear();
    tracker_->clear();
    varHistory_.clear();
  }
  err.clear();
  return true;
}
bool Interpreter::stackRemove(const std::string &name, std::string &err) {
  auto it = variables_.find(name);
  if (it == variables_.end()) {
    // Also check tracker
    auto found = tracker_->find(name);
    if (!found) { err = "variable '" + name + "' not found in stack"; return false; }
  }
  // Find last history entry that defines this variable and undo from there
  // For simplicity, find index of last defining entry and rebuild without it
  int idx = -1;
  for (int i = (int)history_.size()-1; i >=0; --i) {
    std::string type, n, v;
    if (parseDeclaration(history_[i], type, n, v) && n == name) { idx = i; break; }
    std::string aN, aV;
    if (parseAssignment(history_[i], aN, aV) && aN == name) { idx = i; break; }
  }
  if (idx == -1) {
    // Variable is tracked but not in history (maybe from preamble) — just forget
    variables_.erase(name);
    tracker_->forget(name);
    return true;
  }
  // Rebuild history without idx
  std::vector<std::string> newHist;
  for (size_t i=0;i<history_.size();++i) if ((int)i != idx) newHist.push_back(history_[i]);
  std::string local;
  interp_.reset();
  initialized_ = false;
  history_.clear();
  variables_.clear();
  tracker_->clear();
  varHistory_.clear();
  stdLibIncluded_ = false;
  if (!init(currentVersion_, includePaths_, defines_, libraryPaths_, libraries_, local)) {
    err = local;
    return false;
  }
  for (auto &h : newHist) {
    std::string e;
    if (!eval(h, e)) {
      // If replay fails, keep going but report
      err = e;
    }
  }
  return true;
}
bool Interpreter::stackSet(const std::string &name, const std::string &code, std::string &err) {
  // Set/replace variable: remove old then push new code
  // code should be a full declaration like "int x = 42;" or "auto x = 42;"
  std::string dummy;
  stackRemove(name, dummy); // ignore error if not found
  return eval(code, err);
}
bool Interpreter::stackSwap(size_t i, size_t j, std::string &err) {
  if (i >= history_.size() || j >= history_.size()) {
    err = "index out of range (history size " + std::to_string(history_.size()) + ")";
    return false;
  }
  if (i == j) return true;
  std::vector<std::string> newHist = history_;
  std::swap(newHist[i], newHist[j]);
  std::string local;
  interp_.reset();
  initialized_ = false;
  history_.clear();
  variables_.clear();
  tracker_->clear();
  varHistory_.clear();
  stdLibIncluded_ = false;
  if (!init(currentVersion_, includePaths_, defines_, libraryPaths_, libraries_, local)) {
    err = local;
    return false;
  }
  for (auto &h : newHist) {
    std::string e;
    if (!eval(h, e)) {
      err = e;
      return false;
    }
  }
  return true;
}
void Interpreter::setSecurityConfig(const security::SecurityConfig &cfg) {
  sandbox_.setConfig(cfg);
}
security::SecurityConfig Interpreter::securityConfig() const {
  return sandbox_.config();
}
void Interpreter::reset(std::string &err) {
  std::string local;
  interp_.reset();
  initialized_ = false;
  history_.clear();
  variables_.clear();
  tracker_->clear();
  varHistory_.clear();
  stdLibIncluded_ = false;
  if (!init(currentVersion_, local))
    err = local;
  else
    err.clear();
}
void Interpreter::help() const {
  // Show prompt help with color hint when stdout is a tty
  bool useColor = isatty(STDOUT_FILENO) &&
                  !getenv("NO_COLOR") && !getenv("CPP_REPL_NO_COLOR");
  const char *term = getenv("TERM");
  if (useColor && term && std::string(term) == "dumb") useColor = false;
  auto col = [&](const char* code)->std::string { return useColor ? code : ""; };
  auto rst = col("\033[0m");
  auto cyan = col("\033[36m");
  auto grey = col("\033[90m");
  std::cout << "C++ REPL (LLVM VM, O0, no optimizations) ["
            << utils::VersionDetector::toString(currentVersion_)
            << "]\n"
               "Prompt: " + cyan + "cpp" + rst + grey + ":" + rst + cyan + utils::VersionDetector::toString(currentVersion_) + rst + grey + " [n] (time " + rst + col("\033[32m") + "✓" + rst + grey + "/" + rst + col("\033[31m") + "✗" + rst + grey + ")" + rst + grey + ">" + rst + "  colored, shows C++ version, input count & last exec time\n"
               "        use " + grey + "--no-color" + rst + " or " + grey + "NO_COLOR=1" + rst + " to disable, " + grey + "FORCE_COLOR=1" + rst + " to force\n"
                "Commands:\n"
               "  :help  :h       show this help\n"
               "  :quit  :exit :q exit REPL\n"
               "  :dump           dump accumulated inputs\n"
               "  :reset          reset interpreter state\n"
               "  :flush :forget :clearstack :drop  flush stack — clears all definitions (variables no longer exist, can be redefined)\n"
               "  :flush <var>    (future) forget single variable\n"
               "  :clear :cls :c  clear output buffer / terminal screen\n"
               "  :load <file>    load and execute file\n"
               "  :lib <path>     load dynamic library (absolute or relative)\n"
               "  :I <path>       add include search path (abs/rel, like -I)\n"
               "  :L <path>       add library search path (like -L)\n"
               "  :undo [n]       undo last n inputs (default 1)\n"
               "  :version        show current C++ version\n"
               "\n"
               "Enter C++ code. Supports incremental declarations:\n"
               "  cpp> int x = 42;\n"
               "  cpp> x + 1\n"
               "  cpp> #include <iostream>\n"
               "       std::cout << \"hi\" << std::endl;\n"
               "  cpp> int add(int a,int b){return a+b;}\n"
               "  cpp> add(2,3)\n"
               "Include/Lib (cmdline & interactive):\n"
               "  $ cpp-repl -I ./include -I /abs/path -L ./lib -l mylib\n"
               "  $ cpp-repl --include ./include --library m\n"
               "  cpp> :I ./include      // add relative include path\n"
               "  cpp> :I /usr/local/include  // absolute\n"
               "  cpp> #include \"myheader.h\"  // now found via -I\n"
               "  cpp> :lib ./lib/mylib.so   // load absolute/relative lib\n"
               "BigInt: cpp_int / bigint via boost::multiprecision (e.g. "
               "cpp_int a = cpp_int(\"12345678901234567890\"); a*a)\n"
               "C++20/23: auto-detects 'concept', 'requires', 'import' etc. "
               "and switches to -std=c++20/23\n"
               "\n"
               "Multiline: unbalanced { ( [ keeps buffering with " + col("\033[33m") + "...>" + rst + " prompt\n"
               "Timing:  " + grey + "⏱" + rst + " line after each exec + inline in next prompt (e.g. " + grey + "(12.3ms ✓)" + rst + ")\n";
}

} // namespace interpreter
} // namespace cpprepl
