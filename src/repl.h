#ifndef CPP_REPL_REPL_H
#define CPP_REPL_REPL_H

#include <memory>
#include <string>
#include <vector>

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

  // Load and execute a file's contents
  bool loadFile(const std::string &path, std::string &err);

  // Dynamic library loading (wraps Interpreter::LoadDynamicLibrary)
  bool loadLibrary(const std::string &path, std::string &err);

  // Undo last N inputs (wraps Interpreter::Undo)
  bool undo(unsigned n, std::string &err);

  // Try incremental eval: returns true if code was incomplete and should
  // continue buffering (instead of error). Used by REPL loop.
  bool eval(const std::string &code, std::string &err, bool &incomplete);

  size_t historySize() const { return history_.size(); }
  void clearHistory() { history_.clear(); }

private:
  std::unique_ptr<clang::Interpreter> interp_;
  bool initialized_ = false;
  std::vector<std::string> history_;
};

} // namespace repl

#endif // CPP_REPL_REPL_H
