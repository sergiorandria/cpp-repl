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

namespace cpprepl {
namespace interpreter {

Interpreter::Interpreter() = default;
Interpreter::~Interpreter() = default;

bool Interpreter::init(utils::StdVersion version, std::string &err) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  clang::IncrementalCompilerBuilder builder;
  std::vector<const char *> args = {
      utils::VersionDetector::toFlag(version).c_str(), "-O0", "-resource-dir",
      "/usr/lib/clang/22"};
  // Need to keep string storage alive – use static or member. For now use
  // persistent. To avoid c_str() dangling, we duplicate via builder's internal
  // copy? Actually builder stores pointers, so we need to ensure c_str() lives.
  // Use static string.
  static std::string flagStorage;
  flagStorage = utils::VersionDetector::toFlag(version);
  args[0] = flagStorage.c_str();

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
  currentVersion_ = version;

  // Inject BigInt support preamble for python-like large ints (cpp_int +
  // bigint)
  if (utils::BigIntSupport::isAvailable()) {
    clang::Value V;
    auto e = interp_->ParseAndExecute(utils::BigIntSupport::preamble(), &V);
    if (e)
      llvm::handleAllErrors(std::move(e), [&](llvm::ErrorInfoBase &EIB) {});
#ifdef HAS_GMP
    // Also inject GMP mpz support if available (linked with -lgmp)
    auto e2 = interp_->ParseAndExecute(utils::BigIntSupport::gmpPreamble(), &V);
    if (e2)
      llvm::handleAllErrors(std::move(e2), [&](llvm::ErrorInfoBase &EIB) {});
#endif
  }
  return true;
}

bool Interpreter::ensureVersion(utils::StdVersion needed, std::string &err) {
  if (!initialized_)
    return init(needed, err);
  if (static_cast<int>(needed) <= static_cast<int>(currentVersion_))
    return true;
  // Need higher version – re-init and replay history
  std::vector<std::string> oldHistory = history_;
  std::string local;
  interp_.reset();
  initialized_ = false;
  history_.clear();
  if (!init(needed, local)) {
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

bool Interpreter::eval(const std::string &code, std::string &err) {
  // auto-detect version first
  auto needed = utils::VersionDetector::detect(code);
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
  std::string trimmed = trim_copy(code);
  if (trimmed.empty())
    return true;

  std::string toEval = code;
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
      auto e2 = interp_->ParseAndExecute(code, &V2);
      if (!e2) {
        if (V2.isValid()) {
          V2.dump();
          std::cout << "\n";
        }
        if (!code.empty())
          history_.push_back(code);
        return true;
      }
      llvm::handleAllErrors(std::move(e2), [&](llvm::ErrorInfoBase &EIB) {});
    }
    if (!needsSemi && !trimmed.empty() && trimmed.back() != ';' &&
        trimmed.back() != '}' && trimmed.back() != '{') {
      std::string withSemi = trimmed + ";\n";
      clang::Value V2;
      auto e2 = interp_->ParseAndExecute(withSemi, &V2);
      if (!e2) {
        if (V2.isValid()) {
          V2.dump();
          std::cout << "\n";
        }
        if (!code.empty())
          history_.push_back(code);
        return true;
      }
      llvm::handleAllErrors(std::move(e2), [&](llvm::ErrorInfoBase &EIB) {});
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
    std::string t = trim_copy(code);
    if (!t.empty() && t.back() == ';') {
      bool likelyExpr = true;
      if (t.find("int ") != std::string::npos ||
          t.find("auto ") != std::string::npos ||
          t.find("#include") != std::string::npos || t.find("for") == 0 ||
          t.find("while") == 0 || t.find("if") == 0 ||
          t.find("struct ") != std::string::npos ||
          t.find("class ") != std::string::npos ||
          t.find("using ") != std::string::npos ||
          t.find("std::cout") != std::string::npos ||
          t.find("printf") != std::string::npos)
        likelyExpr = false;
      std::string stripped = rtrim_semi(code);
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
  if (!code.empty())
    history_.push_back(code);
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
               "  :lib <path>     load dynamic library\n"
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
               "BigInt: cpp_int / bigint via boost::multiprecision (e.g. "
               "cpp_int a = cpp_int(\"12345678901234567890\"); a*a)\n"
               "C++20/23: auto-detects 'concept', 'requires', 'import' etc. "
               "and switches to -std=c++20/23\n"
               "\n"
               "Multiline: unbalanced { ( [ keeps buffering with ...> prompt\n";
}

} // namespace interpreter
} // namespace cpprepl
