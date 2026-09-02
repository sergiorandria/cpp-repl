#pragma once

/**
 * @file i_command.h
 * @brief i_command — repl module.
 */
#include "cpp-repl/utils/result.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace cpprepl {
namespace repl {

// Command pattern: each REPL command is an ICommand.
// Session holds a CommandRegistry (Strategy + Registry) instead of hardcoded switch-case.
class ICommand {
public:
  virtual ~ICommand() = default;
  virtual std::string name() const = 0;
  virtual std::string description() const = 0;
  virtual utils::Result<void> execute(const std::string &args) = 0;
};

// Registry pattern: maps ":help" -> ICommand, extensible without touching Session
class CommandRegistry {
public:
  using Creator = std::function<std::unique_ptr<ICommand>()>;
  void registerCommand(std::unique_ptr<ICommand> cmd);
  bool has(const std::string &name) const;
  utils::Result<void> execute(const std::string &line) const;
  void help() const;

private:
  std::unordered_map<std::string, std::unique_ptr<ICommand>> cmds_;
};

} // namespace repl
} // namespace cpprepl