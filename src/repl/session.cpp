#include "cpp-repl/repl/session.h"
#include <iostream>
#include <string>

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
      std::cout << "usage: :lib <path>\n";
    else {
      std::string e;
      if (!interp_.loadLibrary(path, e))
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
  if (!t.empty() && t[0] == ':') {
    std::cout << "unknown command: " << line << " (try :help)\n";
    return true;
  }
  return false;
}

void Session::runInteractive() {
  std::string line;
  auto trim = [](std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
      return std::string();
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
  };
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
