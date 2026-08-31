#pragma once
#include <memory>
#include <string>
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/Module.h"

namespace cpprepl {
namespace core {

// Abstract VM interface – scalable, pluggable backends.
class VM {
public:
  virtual ~VM() = default;
  virtual bool init(std::string &err) = 0;
  virtual bool addModule(std::unique_ptr<llvm::Module> M,
                         std::unique_ptr<llvm::LLVMContext> Ctx,
                         std::string &err) = 0;
  virtual llvm::Expected<llvm::orc::ExecutorAddr> lookup(const std::string &name) = 0;
  virtual llvm::orc::LLJIT *getLLJIT() = 0;
};

// Low-level LLJIT implementation – the default VM.
// No optimizations, O0, pure execution.
class LLJITVM : public VM {
public:
  LLJITVM() = default;
  ~LLJITVM() override = default;
  LLJITVM(const LLJITVM &) = delete;
  LLJITVM &operator=(const LLJITVM &) = delete;

  bool init(std::string &err) override;
  bool addModule(std::unique_ptr<llvm::Module> M,
                 std::unique_ptr<llvm::LLVMContext> Ctx,
                 std::string &err) override;
  llvm::Expected<llvm::orc::ExecutorAddr> lookup(const std::string &name) override;
  llvm::orc::LLJIT *getLLJIT() override { return jit_.get(); }

private:
  std::unique_ptr<llvm::orc::LLJIT> jit_;
};

} // namespace core
} // namespace cpprepl
