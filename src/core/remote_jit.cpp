#include "cpp-repl/core/remote_jit.h"
#include "llvm/Support/TargetSelect.h"
#include <filesystem>

namespace cpprepl {
namespace core {

RemoteJITVM::RemoteJITVM(Options opts) : opts_(std::move(opts)), fallback_(std::make_unique<LLJITVM>()) {}
RemoteJITVM::RemoteJITVM() : RemoteJITVM(Options{}) {}
RemoteJITVM::~RemoteJITVM() = default;

bool RemoteJITVM::init(std::string &err) {
  // Try remote if port !=0 and workDir exists, otherwise fallback to LLJIT
  if (opts_.remotePort != 0) {
    std::error_code ec;
    if (!std::filesystem::exists(opts_.workDir, ec)) {
      std::filesystem::create_directories(opts_.workDir, ec);
    }
    // TODO: implement llvm::orc::RemoteEPC via llvm::orc::SelfExecutorProcessControl
    // For now, fallback and mark as non-remote but keep Options for future
    isRemote_ = false;
    // In a full impl we would: auto EPC = SelfExecutorProcessControl::Create(...);
    // auto JTMB = JITTargetMachineBuilder::detectHost();
    // jit_ = LLJIT::Create(std::move(EPC), std::move(JTMB), ...);
  }
  // Fallback to in-process LLJIT (scalable: same interface, no caller change)
  isRemote_ = false;
  return fallback_->init(err);
}

bool RemoteJITVM::addModule(std::unique_ptr<llvm::Module> M, std::unique_ptr<llvm::LLVMContext> Ctx,
                            std::string &err) {
  if (jit_) {
    // Remote path (future)
    return fallback_->addModule(std::move(M), std::move(Ctx), err);
  }
  return fallback_->addModule(std::move(M), std::move(Ctx), err);
}

llvm::Expected<llvm::orc::ExecutorAddr> RemoteJITVM::lookup(const std::string &name) {
  if (jit_) {
    return jit_->lookup(name);
  }
  return fallback_->lookup(name);
}

llvm::orc::LLJIT *RemoteJITVM::getLLJIT() {
  if (jit_) return jit_.get();
  return fallback_->getLLJIT();
}

std::unique_ptr<VM> createVM(const std::string &backend, std::string &err) {
  if (backend == "remote" || backend == "RemoteJIT") {
    auto vm = std::make_unique<RemoteJITVM>();
    if (!vm->init(err)) return nullptr;
    return vm;
  }
  if (backend == "lljit" || backend == "LLJIT" || backend.empty()) {
    auto vm = std::make_unique<LLJITVM>();
    if (!vm->init(err)) return nullptr;
    return vm;
  }
  err = "unknown VM backend: " + backend + " (use lljit or remote)";
  return nullptr;
}

} // namespace core
} // namespace cpprepl
