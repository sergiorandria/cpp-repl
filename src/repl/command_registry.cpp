#include "cpp-repl/repl/i_command.h"

#include <iostream>

namespace cpprepl {
namespace repl {

void CommandRegistry::registerCommand(std::unique_ptr<ICommand> cmd) {
  auto n = cmd->name();
  cmds_[n] = std::move(cmd);
  // Also register short aliases without ':' for convenience
}

bool CommandRegistry::has(const std::string &name) const {
  return cmds_.find(name) != cmds_.end();
}

utils::Result<void> CommandRegistry::execute(const std::string &line) const {
  // Extract command name: first word after ':'
  std::string t = line;
  size_t a = t.find_first_not_of(" \t\r\n");
  if (a != std::string::npos)
    t = t.substr(a);
  else
    t = "";
  if (t.empty() || t[0] != ':')
    return utils::Result<void>::failure("not a command");
  size_t sp = t.find(' ');
  std::string name = (sp == std::string::npos) ? t : t.substr(0, sp);
  std::string args = (sp == std::string::npos) ? "" : t.substr(sp + 1);
  auto it = cmds_.find(name);
  if (it == cmds_.end()) {
    return utils::Result<void>::failure("unknown command: " + line + " (try :help)");
  }
  return it->second->execute(args);
}

void CommandRegistry::help() const {
  for (auto &kv : cmds_) {
    std::cout << "  " << kv.first << " — " << kv.second->description() << "\n";
  }
}

} // namespace repl
} // namespace cpprepl
