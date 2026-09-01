/**
 * @file repl.h
 * @brief Legacy high-level REPL wrapper.
 */
#ifndef CPP_REPL_REPL_H
#define CPP_REPL_REPL_H

#include <memory>
#include <string>
#include <vector>

namespace clang {
class Interpreter;
}

namespace repl {

/**
 * @brief High-level C++ REPL built on clang::Interpreter.
 *
 * Uses llvm::orc::LLJIT internally, O0 and correctness first.
 */
class Repl {
public:
  Repl();
  ~Repl();

  Repl(const Repl &) = delete;
  Repl &operator=(const Repl &) = delete;

  /** @brief Initialize the interpreter. */
  bool init(std::string &err);

  /**
   * @brief Execute a REPL line or block and print Value if produced.
   * @param code Input code.
   * @param err Output error.
   * @return true on success.
   */
  bool eval(const std::string &code, std::string &err);

  /** @brief Dump history. */
  void dump() const;
  /** @brief Reset state. */
  void reset(std::string &err);
  /** @brief Print help. */
  void help() const;

  /** @brief Load and execute a file. */
  bool loadFile(const std::string &path, std::string &err);

  /** @brief Load a dynamic library. */
  bool loadLibrary(const std::string &path, std::string &err);

  /** @brief Undo last N inputs. */
  bool undo(unsigned n, std::string &err);

  /**
   * @brief Eval with incomplete detection for REPL loop.
   * @param code Input code.
   * @param err Output error.
   * @param incomplete Set if more input is needed.
   * @return true if handled.
   */
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
