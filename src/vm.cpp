/**
 * @file vm.cpp
 * @brief Legacy VM wrapper for llvm::orc::LLJIT.
 */
#include "vm.h"

#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"

namespace vm {

VM::VM() = default;
VM::~VM() = default;

bool VM::init(std::string &err) {
  // Initialize native target for JIT execution – low level setup.
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

bool VM::addModule(std::unique_ptr<llvm::Module> M, std::unique_ptr<llvm::LLVMContext> Ctx,
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

llvm::Expected<llvm::orc::ExecutorAddr> VM::lookup(const std::string &name) {
  if (!jit_)
    return llvm::createStringError(llvm::inconvertibleErrorCode(), "VM not initialized");
  return jit_->lookup(name);
}

} // namespace vm
