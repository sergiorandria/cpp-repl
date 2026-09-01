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

  /** @brief Whether to use ANSI color for prompt. */
  bool shouldUseColor(bool forReadline) const;
  /** @brief Format duration in ms for display. */
  std::string formatDuration(double ms) const;
  /** @brief Build primary prompt string. */
  std::string buildPrimaryPrompt(bool forReadline) const;
  /** @brief Build continuation prompt string. */
  std::string buildContinuationPrompt(bool forReadline) const;
  /** @brief Print timing line after execution. */
  void printTimingLine(bool success, double ms) const;
  /** @brief Print highlighted echo of executed code (if color enabled). */
  void printHighlightedEcho(const std::string &code) const;

  interpreter::Interpreter &interp_; ///< Bound interpreter instance.
  std::string buffer_; ///< Current multiline buffer.
  int promptCount_ = 1; ///< Prompt counter for numbered prompts.
  double lastDurationMs_ = 0.0; ///< Last execution time in ms.
  bool lastSuccess_ = true; ///< Last execution success flag.
  bool hasLastTiming_ = false; ///< Whether timing is available.
};

} // namespace repl
} // namespace cpprepl
