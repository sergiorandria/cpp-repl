/**
 * @file vm.cpp
 * @brief LLJITVM implementation - thin wrapper over llvm::orc::LLJIT.
 */
#include "cpp-repl/core/vm.h"

#include "llvm/Support/Error.h"
#include "llvm/Support/TargetSelect.h"

namespace cpprepl {
namespace core {

bool LLJITVM::init(std::string &err) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
  auto jitOrErr = llvm::orc::LLJITBuilder().create();
  if (!jitOrErr) {
    llvm::handleAllErrors(jitOrErr.takeError(),
                          [&](llvm::ErrorInfoBase &EIB) { err = EIB.message(); });
    return false;
  }
  jit_ = std::move(*jitOrErr);
  return true;
}

bool LLJITVM::addModule(std::unique_ptr<llvm::Module> M, std::unique_ptr<llvm::LLVMContext> Ctx,
                        std::string &err) {
  if (!jit_) {
    err = "VM not initialized";
    return false;
  }
  llvm::orc::ThreadSafeModule tsm(std::move(M), std::move(Ctx));
  if (auto e = jit_->addIRModule(std::move(tsm))) {
    llvm::handleAllErrors(std::move(e), [&](llvm::ErrorInfoBase &EIB) { err = EIB.message(); });
    return false;
  }
  return true;
}

llvm::Expected<llvm::orc::ExecutorAddr> LLJITVM::lookup(const std::string &name) {
  if (!jit_)
    return llvm::createStringError(llvm::inconvertibleErrorCode(), "VM not initialized");
  return jit_->lookup(name);
}

} // namespace core
} // namespace cpprepl
