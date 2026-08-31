#ifndef CPP_REPL_REPL_H
#define CPP_REPL_REPL_H

#include <memory>
#include <string>

namespace clang {
class Interpreter;
}

namespace repl {

/// High-level C++ REPL built on top of the low-level VM (clang::Interpreter
/// which internally uses llvm::orc::LLJIT). No optimization, correctness first.
class Repl {
public:
  Repl();
  ~Repl();

  Repl(const Repl &) = delete;
  Repl &operator=(const Repl &) = delete;

  bool init(std::string &err);

  // Execute a single REPL line / block. If Value is produced, it is printed.
  // Returns false on error, true on success.
  bool eval(const std::string &code, std::string &err);

  // REPL commands
  void dump() const;
  void reset(std::string &err);
  void help() const;

private:
  std::unique_ptr<clang::Interpreter> interp_;
  bool initialized_ = false;
};

} // namespace repl

#endif // CPP_REPL_REPL_H
