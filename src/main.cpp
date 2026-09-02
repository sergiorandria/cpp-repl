/**
 * @file main.cpp
 * @brief Entry point for cpp-repl - CLI, VM scaffold, and REPL session.
 */
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

#include <cstdlib>
#include <iostream>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#include <sys/resource.h>
#endif

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
    llvm::FunctionType *FT = llvm::FunctionType::get(builder.getInt32Ty(), false);
    llvm::Function *F =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "scaffold_fn", mod.get());
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
      llvm::handleAllErrors(addrOrErr.takeError(),
                            [&](llvm::ErrorInfoBase &EIB) { msg = EIB.message(); });
      std::cerr << "lookup failed: " << msg << "\n";
      return 1;
    }
    auto fn = addrOrErr->toPtr<int (*)()>();
    std::cout << "scaffold_fn() via VM = " << fn() << " (expected 42)\n";
    std::cout << "VM is scalable: LLJITVM implements core::VM interface, "
                 "pluggable backends possible\n\n";
  }

  // --- C++ REPL (interpreter with auto version + bigint + include/lib) ---
  cpprepl::interpreter::Interpreter interp;
  std::string err;
  // Start with C++23 (np.hpp and modern headers need it)
  // Will auto-downgrade if needed, but C++23 is safest for np headers
  // Pass include/library paths from cmdline (absolute & relative)
  if (!interp.init(cpprepl::utils::StdVersion::Cpp23, opts.includePaths, opts.defines,
                   opts.libraryPaths, opts.libraries, err)) {
    std::cerr << "REPL init failed: " << err << "\n";
    return 1;
  }
  if (!opts.includePaths.empty()) {
    std::cout << "[include paths:";
    for (auto &p : opts.includePaths)
      std::cout << " " << p;
    std::cout << "]\n";
  }
  if (!opts.libraryPaths.empty() || !opts.libraries.empty()) {
    std::cout << "[library paths:";
    for (auto &p : opts.libraryPaths)
      std::cout << " " << p;
    std::cout << " | libs:";
    for (auto &l : opts.libraries)
      std::cout << " " << l;
    std::cout << "]\n";
  }

  // Helper to show stats after file/-e when --stats/--mem/--verbose is given
  auto showFileStats = [&](const std::string &label, std::chrono::steady_clock::time_point t0,
                           std::chrono::steady_clock::time_point t1, bool success) {
    bool needStats = opts.showStats || opts.verbose;
    bool needMem = opts.showMem || opts.verbose;
    bool needTime = opts.showTime || opts.verbose || needStats;
    if (!needStats && !needMem && !needTime) return;
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    bool useColor = !opts.noColor && isatty(STDOUT_FILENO) && !getenv("NO_COLOR") && !getenv("CPP_REPL_NO_COLOR");
    const char *term = getenv("TERM");
    if (useColor && term && std::string(term) == "dumb") useColor = false;
    auto col = [&](const char* c){ return useColor ? std::string(c) : std::string(""); };
    auto rst = col("\033[0m");
    auto cyan = col("\033[36m");
    auto grey = col("\033[90m");
    auto green = col("\033[32m");
    auto red = col("\033[31m");
    auto yellow = col("\033[33m");
    std::cout << cyan << "┌─[file]─ " << rst << label << " " << (success ? green + "✓" + rst : red + "✗" + rst);
    if (needTime) std::cout << grey << "  " << ms << "ms" << rst;
    std::cout << "\n";
    if (needStats || needMem) {
      // Show stack layout (variables, history) and heap layout
      // Use interpreter's stackLayout/heapLayout which already handle color
      if (needStats) {
        interp.stackLayout();
      }
      if (needMem) {
        interp.heapLayout();
      } else if (needStats) {
        // For --stats without --mem, still show a compact memory line
        void* heapTop = sbrk(0);
        std::cout << grey << "│ " << rst << "Heap top: " << yellow << heapTop << rst << "  RSS: ";
        struct rusage ru;
        if (getrusage(RUSAGE_SELF, &ru) == 0) std::cout << green << ru.ru_maxrss << " KB" << rst;
        std::cout << "\n";
      }
    }
    std::cout << cyan << "└──────────────────────────────────────────────" << rst << "\n";
  };

  // Load files / -e code before interactive loop (like python script)
  for (auto &f : opts.files) {
    auto t0 = std::chrono::steady_clock::now();
    std::string e;
    bool ok = interp.loadFile(f, e);
    auto t1 = std::chrono::steady_clock::now();
    if (!ok) {
      std::cerr << "failed to load " << f << ": " << e << "\n";
      showFileStats(f, t0, t1, false);
      return 1;
    }
    std::cout << "[loaded " << f << "] ["
              << cpprepl::utils::VersionDetector::toString(interp.currentVersion()) << "]\n";
    showFileStats(f, t0, t1, true);
  }
  for (auto &c : opts.execCodes) {
    auto t0 = std::chrono::steady_clock::now();
    std::string e;
    bool ok = interp.evalAuto(c, e);
    auto t1 = std::chrono::steady_clock::now();
    if (!ok) {
      if (!e.empty())
        std::cerr << " -e error: " << e << "\n";
      showFileStats(std::string("-e \"") + c.substr(0, 40) + (c.size()>40?"...":"") + "\"", t0, t1, false);
      return 1;
    }
    showFileStats(std::string("-e \"") + c.substr(0, 40) + (c.size()>40?"...":"") + "\"", t0, t1, true);
  }
  if (opts.noInteractive)
    return 0;

  // Honor --no-color early (sets env for Session::shouldUseColor)
  if (opts.noColor) {
    setenv("CPP_REPL_NO_COLOR", "1", 1);
    setenv("NO_COLOR", "1", 1);
  }

  // Show help and current capabilities (with subtle color when enabled)
  {
    bool useColor = !opts.noColor && isatty(STDOUT_FILENO) && !getenv("NO_COLOR") &&
                    !getenv("CPP_REPL_NO_COLOR");
    const char *term = getenv("TERM");
    if (useColor && term && std::string(term) == "dumb")
      useColor = false;
    if (useColor) {
      std::cout << "\033[90m— cpp-repl \033[1;36mC++ REPL\033[0m\033[90m "
                   "(LLVM 22, O0) — type \033[33m:help\033[90m, "
                   "\033[33m:quit\033[90m, prompt shows \033[36mcpp:C++\033[90m"
                   " version, [n] and \033[32m⏱ time\033[90m —\033[0m\n";
    }
  }
  interp.help();
  std::cout << "BigInt available: "
            << (cpprepl::utils::BigIntSupport::isAvailable()
                    ? "yes (boost::multiprecision::cpp_int)"
                    : "no")
            << "\n";
  std::cout << "Try: bigint x = cpp_int(\"123456789012345678901234567890\"); x * x\n";

  // Run interactive session via scalable repl::Session
  cpprepl::repl::Session session(interp);
  session.runInteractive();
  return 0;
}
