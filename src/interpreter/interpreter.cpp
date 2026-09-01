#include "cpp-repl/interpreter/interpreter.h"
#include "cpp-repl/utils/bigint.h"
#include "cpp-repl/utils/version_detector.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Interpreter/Interpreter.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <algorithm>

namespace cpprepl {
namespace interpreter {

Interpreter::Interpreter() = default;
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

  clang::IncrementalCompilerBuilder builder;
  compilerArgsStorage_.clear();
  compilerArgsStorage_.push_back(utils::VersionDetector::toFlag(version));
  compilerArgsStorage_.push_back("-O0");
  compilerArgsStorage_.push_back("-resource-dir");
  compilerArgsStorage_.push_back("/usr/lib/clang/22");
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
  varHistory_.clear();
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

bool Interpreter::addIncludePath(const std::string &path, std::string &err) {
  for (auto &p : includePaths_) if (p == path) return true;
  includePaths_.push_back(path);
  return reinitWithCurrentOptions(err);
}
bool Interpreter::addLibraryPath(const std::string &path, std::string &err) {
  for (auto &p : libraryPaths_) if (p == path) return true;
  libraryPaths_.push_back(path);
  return reinitWithCurrentOptions(err);
}
bool Interpreter::addLibrary(const std::string &lib, std::string &err) {
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
  varHistory_.clear();
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

bool Interpreter::evalAuto(const std::string &code, std::string &err) {
  auto needed = utils::VersionDetector::detect(code);
  if (!ensureVersion(needed, err))
    return false;
  return eval(code, err);
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
      R"(^\s*((?:(?:const|constexpr|static|volatile|inline|extern|mutable)\s+)*)([\w:\<\>\,\s\*\&]+?)\s+(\w+)\s*=\s*(.+?)\s*;?\s*$)",
      std::regex::ECMAScript);
  std::smatch m;
  if (!std::regex_match(t, m, declRegex)) return false;
  std::string qualifiers = trim_copy(m[1].str());
  std::string rawType = trim_copy(m[2].str());
  std::string rawName = trim_copy(m[3].str());
  std::string rawVal = trim_copy(m[4].str());
  if (rawType.empty()) return false;
  if (rawName == "if" || rawName == "for" || rawName == "while" || rawName == "return")
    return false;
  std::string fullType = trim_copy(qualifiers + (qualifiers.empty() ? "" : " ") + rawType);
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
    auto it = variables_.find(name);
    if (it != variables_.end()) {
      std::string prevType = it->second.first;
      std::string prevVal = it->second.second;
      if (prevType == type && prevVal == value) {
        std::cout << "[ignored: redefinition of '" << name << "' with same value " << value << " (type " << type << ")]\n";
        return false;
      } else {
        err = "redefinition of '" + name + "' with different value (previous: " + prevVal + " [" + prevType + "] vs new: " + value + " [" + type + "])";
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
    return;
  }
  std::string aName, aVal;
  if (parseAssignment(trimmed, aName, aVal)) {
    auto it = variables_.find(aName);
    if (it != variables_.end()) {
      varHistory_.push_back(variables_);
      it->second.second = aVal;
    }
  }
}

bool Interpreter::eval(const std::string &code, std::string &err) {
  std::string sanitized = sanitizeIncludes(code);
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
        err = "fatal error: '" + incPath + "' is a directory, not a file [hint] Did you mean '" + incPath + "/np.hpp'? Use -I /home/sergio/Project/Numpy-C-API/include and #include \"np/np.hpp\" or #include <np/np.hpp>. Available: ";
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
  if (!trimmed.empty() && trimmed.back() != ';' && trimmed.back() != '}' &&
      trimmed.back() != '{' && trimmed[0] != '#') {
    bool isDecl =
        trimmed.find('=') != std::string::npos ||
        trimmed.rfind("int ", 0) == 0 || trimmed.rfind("auto ", 0) == 0 ||
        trimmed.rfind("float ", 0) == 0 || trimmed.rfind("double ", 0) == 0 ||
        trimmed.rfind("char ", 0) == 0 || trimmed.rfind("std::", 0) == 0 ||
        trimmed.rfind("const ", 0) == 0 || trimmed.rfind("string ", 0) == 0 ||
        trimmed.rfind("long ", 0) == 0 || trimmed.rfind("unsigned ", 0) == 0;
    if (isDecl) {
      toEval = trimmed + ";\n";
      needsSemi = true;
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
          V2.dump();
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
          V2.dump();
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
              if (shouldPrint2) { V2.dump(); std::cout << "\n"; }
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
        msg += "\n[hint] Did you mean \"/.../include/np/np.hpp\"? Use -I /home/sergio/Project/Numpy-C-API/include and #include \"np/np.hpp\" or #include <np/np.hpp>";
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
      V.dump();
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
            V2.dump();
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
  incomplete = false;
  int braces = 0, parens = 0, brackets = 0;
  for (char c : code) {
    if (c == '{')
      ++braces;
    else if (c == '}')
      --braces;
    else if (c == '(')
      ++parens;
    else if (c == ')')
      --parens;
    else if (c == '[')
      ++brackets;
    else if (c == ']')
      --brackets;
  }
  if (braces > 0 || parens > 0 || brackets > 0) {
    incomplete = true;
    return true;
  }
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
  for (auto &h : history_) {
    std::string type, name, value;
    if (parseDeclaration(h, type, name, value)) {
      variables_[name] = {type, value};
    } else {
      std::string aName, aVal;
      if (parseAssignment(h, aName, aVal)) {
        auto it = variables_.find(aName);
        if (it != variables_.end()) it->second.second = aVal;
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
void Interpreter::reset(std::string &err) {
  std::string local;
  interp_.reset();
  initialized_ = false;
  history_.clear();
  variables_.clear();
  varHistory_.clear();
  if (!init(currentVersion_, local))
    err = local;
  else
    err.clear();
}
void Interpreter::help() const {
  std::cout << "C++ REPL (LLVM VM, O0, no optimizations) ["
            << utils::VersionDetector::toString(currentVersion_)
            << "]\n"
               "Commands:\n"
               "  :help  :h       show this help\n"
               "  :quit  :exit :q exit REPL\n"
               "  :dump           dump accumulated inputs\n"
               "  :reset          reset interpreter state\n"
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
               "Multiline: unbalanced { ( [ keeps buffering with ...> prompt\n";
}

} // namespace interpreter
} // namespace cpprepl
