#pragma once
#include <string>
#include "cpp-repl/utils/result.h"
#include "cpp-repl/config/config.h"

namespace cpprepl {
namespace interpreter {

// Strategy pattern: IInterpreter abstracts the execution engine.
// Current impl is ClangInterpreter (wrapping clang::Interpreter + LLJIT).
// Future impls: RemoteInterpreter, WasmInterpreter can be swapped via InterpreterFactory.
class IInterpreter {
public:
  virtual ~IInterpreter() = default;
  virtual utils::Result<void> init(const config::InterpreterConfig &cfg) = 0;
  virtual utils::Result<void> eval(const std::string &code) = 0;
  virtual utils::Result<void> eval(const std::string &code, bool &incomplete) = 0;
  virtual utils::Result<void> loadFile(const std::string &path) = 0;
  virtual utils::Result<void> addIncludePath(const std::string &path) = 0;
  virtual utils::Result<void> addLibraryPath(const std::string &path) = 0;
  virtual utils::Result<void> addLibrary(const std::string &lib) = 0;
  virtual utils::Result<void> undo(unsigned n) = 0;
  virtual utils::Result<void> reset() = 0;
  virtual void dump() const = 0;
  virtual void help() const = 0;
};

// Factory pattern: creates the appropriate interpreter based on config
class InterpreterFactory {
public:
  enum class Backend { Clang, Remote, Mock };
  static std::unique_ptr<IInterpreter> create(Backend b = Backend::Clang);
  static std::unique_ptr<IInterpreter> create(const config::InterpreterConfig &cfg);
};

} // namespace interpreter
} // namespace cpprepl
