#pragma once
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/Module.h"

#include <memory>
#include <string>

namespace cpprepl {
namespace core {

/**
 * @file vm.h
 * @brief Low-level VM abstraction for JIT execution.
 */

/**
 * @brief Abstract VM interface for pluggable JIT backends.
 *
 * Each backend implements module addition and symbol lookup.
 * The default backend is LLJITVM.
 */
class VM {
public:
  virtual ~VM() = default;

  /**
   * @brief Initialize the VM and native target.
   * @param err Output error message on failure.
   * @return true on success.
   */
  virtual bool init(std::string &err) = 0;

  /**
   * @brief Add an LLVM IR module to the JIT.
   * @param M LLVM module to add.
   * @param Ctx LLVM context owning the module.
   * @param err Output error message on failure.
   * @return true on success.
   */
  virtual bool addModule(std::unique_ptr<llvm::Module> M, std::unique_ptr<llvm::LLVMContext> Ctx,
                         std::string &err) = 0;

  /**
   * @brief Lookup a JIT symbol by name.
   * @param name Symbol name as in IR.
   * @return Executor address or error.
   */
  virtual llvm::Expected<llvm::orc::ExecutorAddr> lookup(const std::string &name) = 0;

  /**
   * @brief Get the underlying LLJIT instance if available.
   * @return Pointer to LLJIT or nullptr.
   */
  virtual llvm::orc::LLJIT *getLLJIT() = 0;
};

/**
 * @brief LLJIT-based VM implementation.
 *
 * Uses llvm::orc::LLJIT with O0, no optimizations, for correctness.
 */
class LLJITVM : public VM {
public:
  LLJITVM() = default;
  ~LLJITVM() override = default;
  LLJITVM(const LLJITVM &) = delete;
  LLJITVM &operator=(const LLJITVM &) = delete;

  bool init(std::string &err) override;
  bool addModule(std::unique_ptr<llvm::Module> M, std::unique_ptr<llvm::LLVMContext> Ctx,
                 std::string &err) override;
  llvm::Expected<llvm::orc::ExecutorAddr> lookup(const std::string &name) override;
  llvm::orc::LLJIT *getLLJIT() override {
    return jit_.get();
  }

private:
  std::unique_ptr<llvm::orc::LLJIT> jit_;
};

} // namespace core
} // namespace cpprepl
