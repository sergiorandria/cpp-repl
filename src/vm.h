/**
 * @file vm.h
 * @brief Legacy low-level VM wrapper.
 */
#ifndef CPP_REPL_VM_H
#define CPP_REPL_VM_H

#include <memory>
#include <string>

#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/Module.h"

namespace vm {

/**
 * @brief Low-level VM wrapping llvm::orc::LLJIT.
 *
 * No optimizations, O0, pure execution.
 * Raw execution layer that the C++ REPL builds on.
 */
class VM {
public:
  VM();
  ~VM();

  VM(const VM &) = delete;
  VM &operator=(const VM &) = delete;

  /** @brief Initialize JIT, must be called once. */
  bool init(std::string &err);

  /**
   * @brief Add a module to the JIT.
   * @param M Module to move.
   * @param Ctx Context for the module.
   * @param err Output error.
   * @return true on success.
   */
  bool addModule(std::unique_ptr<llvm::Module> M,
                 std::unique_ptr<llvm::LLVMContext> Ctx, std::string &err);

  /** @brief Lookup symbol address by IR name. */
  llvm::Expected<llvm::orc::ExecutorAddr> lookup(const std::string &name);

  llvm::orc::LLJIT *getLLJIT() { return jit_.get(); }

private:
  std::unique_ptr<llvm::orc::LLJIT> jit_;
};

} // namespace vm

#endif // CPP_REPL_VM_H
