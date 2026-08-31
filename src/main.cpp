#include "vm.h"
#include "repl.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <string>

// Simple scaffold test for Task 1: verify LLVM linkage + VM init.
// Task 1 does not yet need interactive loop – but we provide it.
int main(int argc, char **argv) {
  // --- Low-level VM sanity check: build a tiny IR module manually ---
  {
    std::string err;
    vm::VM vm;
    if (!vm.init(err)) {
      std::cerr << "VM init failed: " << err << "\n";
      return 1;
    }

    auto ctx = std::make_unique<llvm::LLVMContext>();
    auto mod = std::make_unique<llvm::Module>("scaffold", *ctx);
    llvm::IRBuilder<> builder(*ctx);

    // int scaffold_fn() { return 42; }
    llvm::FunctionType *FT =
        llvm::FunctionType::get(builder.getInt32Ty(), false);
    llvm::Function *F =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                               "scaffold_fn", mod.get());
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
    std::cout << "scaffold_fn() via VM = " << fn() << " (expected 42)\n\n";
  }

  // --- C++ REPL (clang Interpreter) ---
  std::string err;
  repl::Repl repl;
  if (!repl.init(err)) {
    std::cerr << "REPL init failed: " << err << "\n";
    return 1;
  }
  repl.help();

  // If arguments contain --no-interactive, just exit after scaffold (for CI)
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--no-interactive")
      return 0;
  }

  std::string line;
  std::string buffer;
  std::cout << "cpp> " << std::flush;
  while (std::getline(std::cin, line)) {
    // Handle REPL commands
    if (line == ":quit" || line == ":exit" || line == ":q") {
      break;
    }
    if (line == ":help" || line == ":h") {
      repl.help();
      std::cout << "cpp> " << std::flush;
      continue;
    }
    if (line == ":dump") {
      repl.dump();
      std::cout << "cpp> " << std::flush;
      continue;
    }
    if (line == ":reset") {
      repl.reset(err);
      if (!err.empty())
        std::cerr << "reset error: " << err << "\n";
      else
        std::cout << "[reset]\n";
      std::cout << "cpp> " << std::flush;
      continue;
    }
    if (line.rfind(":", 0) == 0) {
      std::cout << "unknown command: " << line << " (try :help)\n";
      std::cout << "cpp> " << std::flush;
      continue;
    }

    // Very minimal multiline: if line ends with '\\' or braces unbalanced, accumulate
    buffer += line + "\n";
    // Heuristic: if empty line or line ends with ';' '}' then evaluate.
    // For scaffold we just eval per line – not optimal, but no optimization per spec.
    bool shouldEval = true;
    // If braces unbalanced, keep buffering
    int open = 0;
    for (char c : buffer) {
      if (c == '{')
        ++open;
      else if (c == '}')
        --open;
    }
    if (open > 0) {
      std::cout << "...> " << std::flush;
      shouldEval = false;
    }

    if (shouldEval) {
      std::string evalErr;
      if (!repl.eval(buffer, evalErr)) {
        if (!evalErr.empty())
          std::cerr << "error: " << evalErr << "\n";
      }
      buffer.clear();
      std::cout << "cpp> " << std::flush;
    }
  }

  std::cout << "\nbye\n";
  return 0;
}
