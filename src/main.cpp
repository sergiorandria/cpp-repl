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
  auto trim = [](std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
      return std::string();
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
  };

  std::cout << "cpp> " << std::flush;
  while (std::getline(std::cin, line)) {
    std::string t = trim(line);

    // Handle REPL commands only when not inside multiline buffer
    if (buffer.empty()) {
      if (t == ":quit" || t == ":exit" || t == ":q" || t == ":quit()" ||
          t == "exit" || t == "quit") {
        break;
      }
      if (t == ":help" || t == ":h") {
        repl.help();
        std::cout << "cpp> " << std::flush;
        continue;
      }
      if (t == ":dump") {
        repl.dump();
        std::cout << "cpp> " << std::flush;
        continue;
      }
      if (t == ":reset") {
        repl.reset(err);
        if (!err.empty())
          std::cerr << "reset error: " << err << "\n";
        else
          std::cout << "[reset]\n";
        std::cout << "cpp> " << std::flush;
        continue;
      }
      if (t.rfind(":load", 0) == 0) {
        std::string path = trim(t.substr(5));
        if (path.empty()) {
          std::cout << "usage: :load <file>\n";
        } else {
          std::string loadErr;
          if (!repl.loadFile(path, loadErr)) {
            std::cerr << "load error: " << loadErr << "\n";
          } else {
            std::cout << "[loaded " << path << "]\n";
          }
        }
        std::cout << "cpp> " << std::flush;
        continue;
      }
      if (!t.empty() && t[0] == ':' ) {
        std::cout << "unknown command: " << line << " (try :help)\n";
        std::cout << "cpp> " << std::flush;
        continue;
      }
      if (t.empty()) {
        std::cout << "cpp> " << std::flush;
        continue;
      }
    }

    // Multiline handling: accumulate until braces/parens balanced
    buffer += line + "\n";

    // Quick incomplete detection via Repl helper
    bool incomplete = false;
    // Use a temporary eval to check incomplete without actually executing?
    // Instead we use simple heuristic here: if braces/parens/brackets unbalanced
    // or line ends with '\' then keep buffering.
    if (!line.empty() && line.back() == '\\') {
      incomplete = true;
    } else {
      int braces = 0, parens = 0, brackets = 0;
      for (char c : buffer) {
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
      } else {
        // Also handle trailing incomplete like "int foo() {" already covered
        // For expressions without terminator, we allow eval – clang will error if incomplete
        incomplete = false;
      }
    }

    if (incomplete) {
      std::cout << "...> " << std::flush;
      continue;
    }

    // Evaluate buffer
    {
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
