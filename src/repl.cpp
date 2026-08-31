#include "repl.h"

#include "clang/Interpreter/Interpreter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/Error.h"
#include <iostream>

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
    // Clang diagnostics already printed to stderr via CompilerInstance.
    return false;
  }
  if (V.isValid()) {
    V.dump();
    std::cout << "\n";
  }
  return true;
}

void Repl::dump() const {
  // For now, no direct IR dump – placeholder. Interpreter keeps PTUs.
  // Future: iterate PTUs and dump LLVM module.
  std::cout << "[dump] IR dump not yet implemented – PTUs kept internally\n";
}

void Repl::reset(std::string &err) {
  std::string local;
  // Re-init interpreter from scratch – simplest reset.
  interp_.reset();
  initialized_ = false;
  if (!init(local)) {
    err = local;
  }
}

void Repl::help() const {
  std::cout << "C++ REPL (LLVM VM, O0, no optimizations)\n"
               "Commands:\n"
               "  :help  :h       show this help\n"
               "  :quit  :exit :q exit REPL\n"
               "  :dump           dump IR (TODO)\n"
               "  :reset          reset interpreter state\n"
               "\n"
               "Enter C++ code. Supports incremental declarations:\n"
               "  cpp> int x = 42;\n"
               "  cpp> x + 1\n"
               "  cpp> #include <iostream>\n"
               "       std::cout << \"hi\" << std::endl;\n";
}

} // namespace repl
