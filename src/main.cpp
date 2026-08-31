#include "cpp-repl/cli/cli.h"
#include "cpp-repl/core/vm.h"
#include "cpp-repl/interpreter/interpreter.h"
#include "cpp-repl/repl/session.h"
#include "cpp-repl/utils/bigint.h"
#include "cpp-repl/utils/version_detector.h"

// Legacy headers kept for compat (wrappers)
// #include "vm.h"
// #include "repl.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <string>

int main(int argc, char **argv) {
  std::string cliErr;
  auto opts = cpprepl::cli::parse(argc, argv, cliErr);
  if (!cliErr.empty()) {
    std::cerr << cliErr << "\n";
    cpprepl::cli::printHelp(argv[0]);
    return 1;
  }
  if (opts.showHelp) {
    cpprepl::cli::printHelp(argv[0]);
    return 0;
  }
  if (opts.showVersion) {
    cpprepl::cli::printVersion();
    return 0;
  }

  // --- Low-level VM scaffold (scalable core) ---
  if (opts.showScaffold) {
    std::string err;
    cpprepl::core::LLJITVM vm;
    if (!vm.init(err)) {
      std::cerr << "VM init failed: " << err << "\n";
      return 1;
    }
    auto ctx = std::make_unique<llvm::LLVMContext>();
    auto mod = std::make_unique<llvm::Module>("scaffold", *ctx);
    llvm::IRBuilder<> builder(*ctx);
    llvm::FunctionType *FT =
        llvm::FunctionType::get(builder.getInt32Ty(), false);
    llvm::Function *F = llvm::Function::Create(
        FT, llvm::Function::ExternalLinkage, "scaffold_fn", mod.get());
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*ctx, "entry", F);
    builder.SetInsertPoint(BB);
    builder.CreateRet(builder.getInt32(42));
    std::string ir;
    llvm::raw_string_ostream os(ir);
    mod->print(os, nullptr);
    std::cout << "=== Scaffold IR (low-level VM) ===\n" << os.str() << "\n";
    if (!vm.addModule(std::move(mod), std::move(ctx), err)) {
      std::cerr << "addModule failed: " << err << "\n";
      return 1;
    }
    auto addrOrErr = vm.lookup("scaffold_fn");
    if (!addrOrErr) {
      std::string msg;
      llvm::handleAllErrors(
          addrOrErr.takeError(),
          [&](llvm::ErrorInfoBase &EIB) { msg = EIB.message(); });
      std::cerr << "lookup failed: " << msg << "\n";
      return 1;
    }
    auto fn = addrOrErr->toPtr<int (*)()>();
    std::cout << "scaffold_fn() via VM = " << fn() << " (expected 42)\n";
    std::cout << "VM is scalable: LLJITVM implements core::VM interface, "
                 "pluggable backends possible\n\n";
  }

  // --- C++ REPL (interpreter with auto version + bigint) ---
  cpprepl::interpreter::Interpreter interp;
  std::string err;
  // Start with C++17, will auto-upgrade to C++20/23 when needed
  if (!interp.init(cpprepl::utils::StdVersion::Cpp17, err)) {
    std::cerr << "REPL init failed: " << err << "\n";
    return 1;
  }

  // Load files / -e code before interactive loop (like python script)
  for (auto &f : opts.files) {
    std::string e;
    if (!interp.loadFile(f, e)) {
      std::cerr << "failed to load " << f << ": " << e << "\n";
      return 1;
    }
    std::cout << "[loaded " << f << "] ["
              << cpprepl::utils::VersionDetector::toString(
                     interp.currentVersion())
              << "]\n";
  }
  for (auto &c : opts.execCodes) {
    std::string e;
    if (!interp.evalAuto(c, e)) {
      if (!e.empty())
        std::cerr << " -e error: " << e << "\n";
      return 1;
    }
  }
  if (opts.noInteractive)
    return 0;

  // Show help and current capabilities
  interp.help();
  std::cout << "BigInt available: "
            << (cpprepl::utils::BigIntSupport::isAvailable()
                    ? "yes (boost::multiprecision::cpp_int)"
                    : "no")
            << "\n";
  std::cout
      << "Try: bigint x = cpp_int(\"123456789012345678901234567890\"); x * x\n";

  // Run interactive session via scalable repl::Session
  cpprepl::repl::Session session(interp);
  session.runInteractive();
  return 0;
}
