#pragma once
#include <string>
#include "cpp-repl/interpreter/interpreter.h"

namespace cpprepl {
namespace repl {

// Scalable REPL session – handles prompts, multiline, commands.
// Like Python's REPL, but for C++ raw code.
class Session {
public:
  explicit Session(interpreter::Interpreter &interp);
  ~Session() = default;

  // Run interactive loop reading from stdin
  void runInteractive();

  // Execute single line (used for -e and tests)
  bool exec(const std::string &code, std::string &err);

private:
  bool handleCommand(const std::string &line, std::string &err);
  bool isIncomplete(const std::string &buffer) const;

  interpreter::Interpreter &interp_;
  std::string buffer_;
};

} // namespace repl
} // namespace cpprepl
