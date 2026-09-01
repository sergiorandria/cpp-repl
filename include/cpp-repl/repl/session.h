#pragma once
#include <string>
#include "cpp-repl/interpreter/interpreter.h"

namespace cpprepl {
namespace repl {

/**
 * @file session.h
 * @brief Interactive REPL session handling.
 */

/**
 * @brief Manages the interactive prompt, history, and commands.
 *
 * Delegates execution to interpreter::Interpreter and provides Python-like
 * REPL behavior for raw C++ input.
 */
class Session {
public:
  /**
   * @brief Construct a session bound to an interpreter.
   * @param interp Reference to initialized interpreter.
   */
  explicit Session(interpreter::Interpreter &interp);
  ~Session() = default;

  /**
   * @brief Run the interactive loop reading from stdin.
   *
   * Handles prompts cpp> / ...>, line buffering, and :commands.
   */
  void runInteractive();

  /**
   * @brief Execute a single code snippet.
   * @param code Raw C++ code.
   * @param err Output error message.
   * @return true on success.
   */
  bool exec(const std::string &code, std::string &err);

private:
  /**
   * @brief Handle a colon command like :help or :I.
   * @param line Input line.
   * @param err Output error.
   * @return true if line was a command.
   */
  bool handleCommand(const std::string &line, std::string &err);
  /**
   * @brief Check if buffer has unbalanced braces/parens.
   * @param buffer Current input buffer.
   * @return true if more input is needed.
   */
  bool isIncomplete(const std::string &buffer) const;

  interpreter::Interpreter &interp_;
  std::string buffer_;
};

} // namespace repl
} // namespace cpprepl
