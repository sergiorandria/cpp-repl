#include "cpp-repl/repl/session.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <unistd.h>
#ifdef HAS_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

namespace cpprepl {
namespace repl {

Session::Session(interpreter::Interpreter &interp) : interp_(interp) {}

bool Session::exec(const std::string &code, std::string &err) {
  return interp_.eval(code, err);
}

bool Session::isIncomplete(const std::string &buffer) const {
  int braces = 0, parens = 0, brackets = 0;
  for (char c : buffer) {
    if (c == '{')
      ++braces;
    else if (c == '}')
      --braces;
    else if (c == '(')
      ++parens;
    else if (c == ')')
      --parens;
    else if (c == '[')
      ++brackets;
    else if (c == ']')
      --brackets;
  }
  return braces > 0 || parens > 0 || brackets > 0;
}

bool Session::handleCommand(const std::string &line, std::string &err) {
  auto trim = [](std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
      return std::string();
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
  };
  std::string t = trim(line);
  if (t == ":help" || t == ":h") {
    interp_.help();
    return true;
  }
  if (t == ":dump") {
    interp_.dump();
    return true;
  }
  if (t == ":version") {
    std::cout << "Current: " << (int)interp_.currentVersion() << "\n";
    return true;
  }
  if (t == ":reset") {
    interp_.reset(err);
    if (!err.empty())
      std::cerr << "reset error: " << err << "\n";
    else
      std::cout << "[reset]\n";
    return true;
  }
  if (t == ":clear" || t == ":cls" || t == ":c" || t == "clear" || t == "cls") {
    // Clear output buffer / terminal screen
    // ANSI clear screen + move cursor home
    std::cout << "\033[2J\033[H" << std::flush;
#ifdef HAS_READLINE
    if (isatty(STDOUT_FILENO)) {
      // readline helper to clear screen and redisplay
      rl_clear_screen(0, 0);
      rl_on_new_line();
    }
#endif
    // Also clear any partially accumulated multiline buffer
    buffer_.clear();
    return true;
  }
  if (t.rfind(":load", 0) == 0) {
    std::string path = trim(t.substr(5));
    if (path.empty())
      std::cout << "usage: :load <file>\n";
    else {
      std::string e;
      if (!interp_.loadFile(path, e))
        std::cerr << "load error: " << e << "\n";
      else
        std::cout << "[loaded " << path << "]\n";
    }
    return true;
  }
  if (t.rfind(":lib", 0) == 0) {
    std::string path = trim(t.substr(4));
    if (path.empty())
      std::cout << "usage: :lib <path> (absolute, relative, or -l name)\n";
    else {
      std::string e;
      // Use addLibrary for bare names (searches -L paths), else loadLibrary for direct path
      bool ok = false;
      if (path.find('/') != std::string::npos || path.find(".so") != std::string::npos) {
        ok = interp_.loadLibrary(path, e);
        if (!ok) ok = interp_.addLibrary(path, e);
      } else {
        ok = interp_.addLibrary(path, e);
      }
      if (!ok)
        std::cerr << "lib error: " << e << "\n";
      else
        std::cout << "[lib " << path << "]\n";
    }
    return true;
  }
  if (t.rfind(":undo", 0) == 0) {
    std::string rest = trim(t.substr(5));
    unsigned n = 1;
    if (!rest.empty()) {
      char *end = nullptr;
      unsigned long v = strtoul(rest.c_str(), &end, 10);
      if (end != rest.c_str() && v > 0)
        n = (unsigned)v;
    }
    std::string e;
    if (!interp_.undo(n, e))
      std::cerr << "undo error: " << e << "\n";
    else
      std::cout << "[undid " << n << "]\n";
    return true;
  }
  // Include path handling (absolute & relative) – interactive :I / :include
  if (t.rfind(":I", 0) == 0 || t.rfind(":include", 0) == 0 || t.rfind(":inc", 0) == 0) {
    std::string path;
    if (t.rfind(":I", 0) == 0) path = trim(t.substr(2));
    else if (t.rfind(":include", 0) == 0) path = trim(t.substr(8));
    else path = trim(t.substr(4));
    // Support ":I=path" or ":I path"
    if (!path.empty() && path[0] == '=') path = trim(path.substr(1));
    if (path.empty()) { std::cout << "usage: :I <path>  (add include path, absolute or relative)\n"; return true; }
    std::string e;
    if (!interp_.addIncludePath(path, e)) std::cerr << "include path error: " << e << "\n";
    else std::cout << "[include path: " << path << "]\n";
    return true;
  }
  if (t.rfind(":L", 0) == 0 || t.rfind(":libpath", 0) == 0) {
    std::string path;
    if (t.rfind(":L", 0) == 0) path = trim(t.substr(2));
    else path = trim(t.substr(8));
    if (!path.empty() && path[0] == '=') path = trim(path.substr(1));
    if (path.empty()) { std::cout << "usage: :L <path>  (add library search path)\n"; return true; }
    std::string e;
    if (!interp_.addLibraryPath(path, e)) std::cerr << "library path error: " << e << "\n";
    else std::cout << "[library path: " << path << "]\n";
    return true;
  }
  if (!t.empty() && t[0] == ':') {
    std::cout << "unknown command: " << line << " (try :help)\n";
    return true;
  }
  return false;
}

void Session::runInteractive() {
  auto trim = [](std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
      return std::string();
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
  };

#ifdef HAS_READLINE
  bool useReadline = isatty(STDIN_FILENO);
  if (useReadline) {
    rl_readline_name = const_cast<char*>("cpp-repl");
    using_history();
    const char *home = getenv("HOME");
    std::string histFile = home ? std::string(home) + "/.cpp_repl_history" : "";
    if (!histFile.empty()) read_history(histFile.c_str());
    char *raw = nullptr;
    while (true) {
      const char *prompt = buffer_.empty() ? "cpp> " : "...> ";
      raw = readline(prompt);
      if (!raw) {
        std::cout << "\nbye\n";
        break;
      }
      std::string line(raw);
      free(raw);
      raw = nullptr;
      std::string t = trim(line);
      if (buffer_.empty()) {
        if (t == ":quit" || t == ":exit" || t == ":q" || t == ":quit()" ||
            t == "exit" || t == "quit") {
          break;
        }
        if (t.empty()) {
          continue;
        }
        std::string err;
        if (handleCommand(line, err)) {
          if (!t.empty()) {
            add_history(line.c_str());
            if (!histFile.empty()) append_history(1, histFile.c_str());
          }
          continue;
        }
      }
      if (!line.empty()) {
        HIST_ENTRY *last = history_get(history_length);
        if (!last || std::string(last->line) != line) {
          add_history(line.c_str());
          if (!histFile.empty()) append_history(1, histFile.c_str());
        }
      }
      buffer_ += line + "\n";
      bool incomplete = false;
      if (!line.empty() && line.back() == '\\')
        incomplete = true;
      else if (isIncomplete(buffer_))
        incomplete = true;
      if (incomplete) {
        continue;
      }
      std::string err;
      if (!interp_.eval(buffer_, err)) {
        if (!err.empty())
          std::cerr << "error: " << err << "\n";
      }
      buffer_.clear();
    }
    if (!histFile.empty()) write_history(histFile.c_str());
    return;
  }
#endif
  std::string line;
  std::cout << "cpp> " << std::flush;
  while (std::getline(std::cin, line)) {
    std::string t = trim(line);
    if (buffer_.empty()) {
      if (t == ":quit" || t == ":exit" || t == ":q" || t == ":quit()" ||
          t == "exit" || t == "quit")
        break;
      if (t.empty()) {
        std::cout << "cpp> " << std::flush;
        continue;
      }
      std::string err;
      if (handleCommand(line, err)) {
        std::cout << "cpp> " << std::flush;
        continue;
      }
    }
    buffer_ += line + "\n";
    bool incomplete = false;
    if (!line.empty() && line.back() == '\\')
      incomplete = true;
    else if (isIncomplete(buffer_))
      incomplete = true;
    if (incomplete) {
      std::cout << "...> " << std::flush;
      continue;
    }
    std::string err;
    if (!interp_.eval(buffer_, err)) {
      if (!err.empty())
        std::cerr << "error: " << err << "\n";
    }
    buffer_.clear();
    std::cout << "cpp> " << std::flush;
  }
  std::cout << "\nbye\n";
}

} // namespace repl
} // namespace cpprepl
