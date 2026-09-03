#pragma once
#include "cpp-repl/core/vm.h"
#include <string>

namespace cpprepl {
namespace core {

/**
 * @file remote_jit.h
 * @brief Remote JIT backend (out-of-process) for sandboxing and scalability.
 * @details Implements VM via llvm::orc::LLJIT with Remote EPC (ExecutorProcessControl).
 *          Falls back to in-process LLJIT when remote not available (e.g. CI).
 *          Enables LSP/notebook concurrency: each cell can be a separate JITDylib.
 */

class RemoteJITVM : public VM {
public:
  struct Options {
    std::string remoteHost = "localhost";
    uint16_t remotePort = 0; // 0 = in-process fallback
    bool useProcessIsolation = true;
    std::string workDir = "/tmp/cpp-repl-remote";
  };
  explicit RemoteJITVM(Options opts);
  RemoteJITVM();
  ~RemoteJITVM() override;

  RemoteJITVM(const RemoteJITVM &) = delete;
  RemoteJITVM(RemoteJITVM &&) = delete;
  RemoteJITVM &operator=(const RemoteJITVM &) = delete;
  RemoteJITVM &operator=(RemoteJITVM &&) = delete;

  bool init(std::string &err) override;
  bool addModule(std::unique_ptr<llvm::Module> M, std::unique_ptr<llvm::LLVMContext> Ctx,
                 std::string &err) override;
  llvm::Expected<llvm::orc::ExecutorAddr> lookup(const std::string &name) override;
  llvm::orc::LLJIT *getLLJIT() override;

  bool isRemote() const { return isRemote_; }
  const Options &options() const { return opts_; }

private:
  Options opts_;
  std::unique_ptr<VM> fallback_; // LLJITVM when remote unavailable
  std::unique_ptr<llvm::orc::LLJIT> jit_;
  bool isRemote_ = false;
};

// Factory helper
std::unique_ptr<VM> createVM(const std::string &backend, std::string &err);

} // namespace core
} // namespace cpprepl
