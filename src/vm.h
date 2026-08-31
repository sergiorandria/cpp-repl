#ifndef CPP_REPL_VM_H
#define CPP_REPL_VM_H

#include <memory>
#include <string>

#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/Module.h"

namespace vm {

/// Low-level VM wrapping llvm::orc::LLJIT.
/// No optimizations – O0, pure execution.
/// This is the raw execution layer; C++ REPL builds on top.
class VM {
public:
  VM();
  ~VM();

  VM(const VM &) = delete;
  VM &operator=(const VM &) = delete;

  // Initialize JIT – must be called once.
  bool init(std::string &err);

  // Add a Module (takes ownership). Module is moved into ThreadSafeModule.
  bool addModule(std::unique_ptr<llvm::Module> M,
                 std::unique_ptr<llvm::LLVMContext> Ctx, std::string &err);

  // Lookup symbol address (mangled IR name)
  llvm::Expected<llvm::orc::ExecutorAddr> lookup(const std::string &name);

  // For debugging: dump current state not needed at this level

  llvm::orc::LLJIT *getLLJIT() { return jit_.get(); }

private:
  std::unique_ptr<llvm::orc::LLJIT> jit_;
};

} // namespace vm

#endif // CPP_REPL_VM_H
