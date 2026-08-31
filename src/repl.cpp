#include "repl.h"

#include "clang/Interpreter/Interpreter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/Error.h"
#include <fstream>
#include <iostream>
#include <sstream>

namespace repl {

Repl::Repl() = default;
Repl::~Repl() = default;

bool Repl::init(std::string &err) {
  // Build incremental compiler – minimal args, O0, no optimization.
  clang::IncrementalCompilerBuilder builder;
  // Bare C++17 with include paths from system. Use default driver args.
  // Explicit resource dir needed so builtin headers (stddef.h) are found.
  std::vector<const char *> args = {"-std=c++17", "-O0",
                                    "-resource-dir", "/usr/lib/clang/22"};
  builder.SetCompilerArgs(args);

  auto CI = builder.CreateCpp();
  if (!CI) {
    llvm::handleAllErrors(CI.takeError(), [&](llvm::ErrorInfoBase &EIB) {
      err = EIB.message();
    });
    return false;
  }

  auto interpOrErr = clang::Interpreter::create(std::move(*CI));
  if (!interpOrErr) {
    llvm::handleAllErrors(interpOrErr.takeError(),
                          [&](llvm::ErrorInfoBase &EIB) { err = EIB.message(); });
    return false;
  }
  interp_ = std::move(*interpOrErr);
  initialized_ = true;
  return true;
}

bool Repl::eval(const std::string &code, std::string &err) {
  if (!initialized_ || !interp_) {
    err = "REPL not initialized";
    return false;
  }
  clang::Value V;
  auto e = interp_->ParseAndExecute(code, &V);
  if (e) {
    llvm::handleAllErrors(std::move(e),
                          [&](llvm::ErrorInfoBase &EIB) { err = EIB.message(); });
    return false;
  }
  if (V.isValid()) {
    V.dump();
    std::cout << "\n";
  }
  if (!code.empty())
    history_.push_back(code);
  return true;
}

bool Repl::eval(const std::string &code, std::string &err, bool &incomplete) {
  incomplete = false;
  // Heuristic: empty is complete
  std::string trimmed = code;
  // Very low-level check: if braces/parens unbalanced, treat as incomplete
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

bool Repl::loadFile(const std::string &path, std::string &err) {
  std::ifstream f(path);
  if (!f) {
    err = "cannot open file: " + path;
    return false;
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  return eval(ss.str(), err);
}

bool Repl::loadLibrary(const std::string &path, std::string &err) {
  if (!initialized_ || !interp_) {
    err = "REPL not initialized";
    return false;
  }
  if (auto e = interp_->LoadDynamicLibrary(path.c_str())) {
    llvm::handleAllErrors(std::move(e),
                          [&](llvm::ErrorInfoBase &EIB) { err = EIB.message(); });
    return false;
  }
  return true;
}

bool Repl::undo(unsigned n, std::string &err) {
  if (!initialized_ || !interp_) {
    err = "REPL not initialized";
    return false;
  }
  if (auto e = interp_->Undo(n)) {
    llvm::handleAllErrors(std::move(e),
                          [&](llvm::ErrorInfoBase &EIB) { err = EIB.message(); });
    return false;
  }
  // Also pop from history
  while (n-- > 0 && !history_.empty())
    history_.pop_back();
  return true;
}

void Repl::dump() const {
  std::cout << "=== REPL history (" << history_.size() << " inputs) ===\n";
  for (size_t i = 0; i < history_.size(); ++i) {
    std::cout << "[" << i << "] " << history_[i];
    if (history_[i].empty() || history_[i].back() != '\n')
      std::cout << "\n";
    std::cout << "---\n";
  }
  if (history_.empty())
    std::cout << "(no inputs yet)\n";
}

void Repl::reset(std::string &err) {
  std::string local;
  interp_.reset();
  initialized_ = false;
  history_.clear();
  if (!init(local)) {
    err = local;
  } else {
    err.clear();
  }
}

void Repl::help() const {
  std::cout << "C++ REPL (LLVM VM, O0, no optimizations)\n"
               "Commands:\n"
               "  :help  :h       show this help\n"
               "  :quit  :exit :q exit REPL\n"
               "  :dump           dump accumulated inputs\n"
               "  :reset          reset interpreter state\n"
               "  :load <file>    load and execute file\n"
               "  :lib <path>     load dynamic library\n"
               "  :undo [n]       undo last n inputs (default 1)\n"
               "\n"
               "Enter C++ code. Supports incremental declarations:\n"
               "  cpp> int x = 42;\n"
               "  cpp> x + 1\n"
               "  cpp> #include <iostream>\n"
               "       std::cout << \"hi\" << std::endl;\n"
               "  cpp> int add(int a,int b){return a+b;}\n"
               "  cpp> add(2,3)\n"
               "\n"
               "Multiline: unbalanced { ( [ keeps buffering with ...> prompt\n"
               "Diagnostics: errors printed to stderr, history not updated on failure\n";
}

} // namespace repl
