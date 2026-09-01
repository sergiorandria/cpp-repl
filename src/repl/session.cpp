/**
 * @file session.cpp
 * @brief Interactive session loop and command handling.
 */
#include "cpp-repl/repl/session.h"
#include <iostream>
#include <string>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
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
  bool inSingle = false, inDouble = false, inLineComment = false, inBlockComment = false;
  bool escaped = false;
  for (size_t i = 0; i < buffer.size(); ++i) {
    char c = buffer[i];
    char n = (i + 1 < buffer.size()) ? buffer[i + 1] : '\0';
    if (inLineComment) {
      if (c == '\n') inLineComment = false;
      continue;
    }
    if (inBlockComment) {
      if (c == '*' && n == '/') { inBlockComment = false; ++i; }
      continue;
    }
    if (inSingle) {
      if (escaped) escaped = false;
      else if (c == '\\') escaped = true;
      else if (c == '\'') inSingle = false;
      continue;
    }
    if (inDouble) {
      if (escaped) escaped = false;
      else if (c == '\\') escaped = true;
      else if (c == '"') inDouble = false;
      continue;
    }
    if (c == '/' && n == '/') { inLineComment = true; ++i; continue; }
    if (c == '/' && n == '*') { inBlockComment = true; ++i; continue; }
    if (c == '\'') { inSingle = true; continue; }
    if (c == '"') { inDouble = true; continue; }
    if (c == '{') ++braces;
    else if (c == '}') --braces;
    else if (c == '(') ++parens;
    else if (c == ')') --parens;
    else if (c == '[') ++brackets;
    else if (c == ']') --brackets;
  }
  if (inDouble || inSingle || inBlockComment) return true;
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
    // Robust cross-platform terminal clear: works on every terminal type
    // 1. isatty check – when piped/redirected there is no screen to clear
    // 2. HAS_READLINE – use terminfo-aware rl_clear_screen when available
    // 3. Windows: Win32 Console API (no external process), fallback to cls
    // 4. POSIX: TERM=dumb -> newlines; TERM set -> `clear` (terminfo),
    //    fallback to ANSI with scrollback clear, fallback to newlines
#ifdef HAS_READLINE
#ifdef _WIN32
    if (_isatty(_fileno(stdin)) || _isatty(_fileno(stdout))) {
#else
    if (isatty(STDOUT_FILENO) || isatty(STDIN_FILENO)) {
#endif
      // readline's clear is terminfo-aware (honors TERM, works in tmux/screen/xterm)
      rl_clear_screen(0, 0);
      rl_on_new_line();
      // Also emit ANSI / system clear so non-readline output and scrollback are cleared
    }
#endif
#ifdef _WIN32
    // Windows: try native Console API first – works even without `cls` in PATH
    // and on all Windows consoles (cmd.exe, PowerShell, Windows Terminal)
    {
      HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
      if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
          COORD topLeft = {0, 0};
          DWORD written = 0;
          DWORD cells = static_cast<DWORD>(csbi.dwSize.X) * static_cast<DWORD>(csbi.dwSize.Y);
          FillConsoleOutputCharacterA(hOut, ' ', cells, topLeft, &written);
          FillConsoleOutputAttribute(hOut, csbi.wAttributes, cells, topLeft, &written);
          SetConsoleCursorPosition(hOut, topLeft);
        } else {
          // API failed (e.g., redirected handle) – fallback
          if (std::system("cls") != 0) {
            std::cout << "\033[2J\033[3J\033[H" << std::flush;
          }
        }
      } else {
        if (std::system("cls") != 0) {
          std::cout << "\033[2J\033[3J\033[H" << std::flush;
        }
      }
    }
#else
    // POSIX: check if we are actually attached to a terminal
    {
      bool isTTY = isatty(STDOUT_FILENO) || isatty(STDERR_FILENO) || isatty(STDIN_FILENO);
      if (!isTTY) {
        // Piped / redirected / non-interactive – simulate clear with newlines
        std::cout << std::string(100, '\n') << std::flush;
      } else {
        const char *term = std::getenv("TERM");
        std::string termStr = term ? term : "";
        if (termStr.empty() || termStr == "dumb") {
          // Dumb terminal (e.g., emacs shell, dumb) – ANSI not supported
          std::cout << std::string(100, '\n') << std::flush;
        } else {
          // Try terminfo-based `clear` – honors TERM (xterm-256color, screen, tmux, etc.)
          // Returns 0 on success; non-zero if `clear` missing or TERM unknown
          int rc = std::system("clear");
          if (rc != 0) {
            // Fallback 1: ANSI – works on virtually all modern terminals
            // \033[2J clear screen, \033[3J clear scrollback, \033[H home cursor
            std::cout << "\033[2J\033[3J\033[H" << std::flush;
            // Fallback 2: if ANSI somehow not honored, ensure visual separation
            // Heuristic: if TERM is very minimal, also push with newlines is redundant
            // so we rely on ANSI alone here. The 100-newline fallback above covers dumb.
          }
        }
      }
    }
#endif
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
      buffer_ += line + "\n";
      bool incomplete = false;
      if (!line.empty() && line.back() == '\\')
        incomplete = true;
      else if (isIncomplete(buffer_))
        incomplete = true;
      if (incomplete) {
        continue;
      }
      // Complete input ready: store entire buffer as single history entry
      // so that up/down navigation recalls whole function bodies at once
      // instead of line-by-line. This makes a multi-line function definition
      // (e.g. "int foo() {\n  return 42;\n}") appear as one history item.
      {
        std::string histEntry = buffer_;
        while (!histEntry.empty() &&
               (histEntry.back() == '\n' || histEntry.back() == '\r'))
          histEntry.pop_back();
        if (!histEntry.empty()) {
          HIST_ENTRY *last = history_get(history_length);
          if (!last || histEntry != last->line) {
            add_history(histEntry.c_str());
            if (!histFile.empty()) append_history(1, histFile.c_str());
          }
        }
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
