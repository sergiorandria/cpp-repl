/**
 * @file session.cpp
 * @brief Interactive session loop and command handling.
 */
#include "cpp-repl/repl/session.h"
#include "cpp-repl/utils/highlight.h"
#include "cpp-repl/utils/incomplete_detector.h"
#include "cpp-repl/utils/version_detector.h"
#include <chrono>
#include <regex>
#include <iomanip>
#include <iostream>
#include <sstream>
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
  auto t0 = std::chrono::steady_clock::now();
  bool ok = interp_.eval(code, err);
  auto t1 = std::chrono::steady_clock::now();
  lastDurationMs_ = std::chrono::duration<double, std::milli>(t1 - t0).count();
  lastSuccess_ = ok;
  hasLastTiming_ = true;
  ++promptCount_;
  return ok;
}

// ── Prompt helpers ──────────────────────────────────────────────────────────
bool Session::shouldUseColor(bool /*forReadline*/) const {
  if (std::getenv("NO_COLOR") || std::getenv("CPP_REPL_NO_COLOR") ||
      std::getenv("NO_COLOUR"))
    return false;
  if (std::getenv("FORCE_COLOR") || std::getenv("CLICOLOR_FORCE"))
    return true;
  const char *term = std::getenv("TERM");
  if (term && std::string(term) == "dumb")
    return false;
#ifdef _WIN32
  return _isatty(_fileno(stdout)) != 0;
#else
  return isatty(STDOUT_FILENO) != 0;
#endif
}

std::string Session::formatDuration(double ms) const {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  if (ms < 1.0) {
    oss << std::setprecision(2) << ms << "ms";
  } else if (ms < 1000.0) {
    oss << std::setprecision(1) << ms << "ms";
  } else if (ms < 60000.0) {
    oss << std::setprecision(2) << (ms / 1000.0) << "s";
  } else {
    int totalSec = static_cast<int>(ms / 1000.0);
    int minutes = totalSec / 60;
    double secs = (ms / 1000.0) - minutes * 60;
    oss << minutes << "m" << std::fixed << std::setprecision(1) << secs << "s";
  }
  return oss.str();
}

std::string Session::buildPrimaryPrompt(bool forReadline) const {
  bool color = shouldUseColor(forReadline);
  auto wrap = [&](const char *code) -> std::string {
    if (!color) return "";
    if (forReadline) return std::string("\001") + code + "\002";
    return std::string(code);
  };
  const std::string RST = wrap("\033[0m");
  const std::string BOLD_CYAN = wrap("\033[1;36m");
  const std::string DIM = wrap("\033[2m");
  const std::string DIM_GREY = wrap("\033[90m");
  const std::string YELLOW = wrap("\033[33m");
  const std::string GREEN = wrap("\033[32m");
  const std::string RED = wrap("\033[31m");

  std::string verStr = utils::VersionDetector::toString(interp_.currentVersion());
  std::string verCol;
  if (interp_.currentVersion() == utils::StdVersion::Cpp23)
    verCol = wrap("\033[1;35m");
  else if (interp_.currentVersion() == utils::StdVersion::Cpp20)
    verCol = YELLOW;
  else
    verCol = DIM_GREY;

  std::string out;
  if (color) {
    out += BOLD_CYAN + "cpp" + RST;
    out += DIM_GREY + ":" + RST + verCol + verStr + RST;
    out += " " + DIM + "[" + std::to_string(promptCount_) + "]" + RST;
    if (hasLastTiming_) {
      std::string t = formatDuration(lastDurationMs_);
      std::string statusCol = lastSuccess_ ? GREEN : RED;
      std::string sym = lastSuccess_ ? "✓" : "✗";
      out += " " + DIM_GREY + "(" + RST + statusCol + t + " " + sym + RST + DIM_GREY + ")" + RST;
    }
    out += DIM_GREY + ">" + RST + " ";
  } else {
    out = "cpp:" + verStr + " [" + std::to_string(promptCount_) + "]";
    if (hasLastTiming_) {
      out += " (" + formatDuration(lastDurationMs_) + (lastSuccess_ ? " ok" : " err") + ")";
    }
    out += "> ";
  }
  return out;
}

std::string Session::buildContinuationPrompt(bool forReadline) const {
  bool color = shouldUseColor(forReadline);
  auto wrap = [&](const char *code) -> std::string {
    if (!color) return "";
    if (forReadline) return std::string("\001") + code + "\002";
    return std::string(code);
  };
  const std::string RST = wrap("\033[0m");
  const std::string DIM_YELLOW = wrap("\033[2;33m");
  if (color) {
    return DIM_YELLOW + "...>" + RST + " ";
  } else {
    return "...> ";
  }
}

void Session::printTimingLine(bool success, double ms) const {
#ifdef _WIN32
  bool isTTY = _isatty(_fileno(stdout)) != 0;
#else
  bool isTTY = isatty(STDOUT_FILENO) != 0;
#endif
  if (!isTTY) return;
  bool color = shouldUseColor(false);
  std::string t = formatDuration(ms);
  if (color) {
    const char *grey = "\033[90m";
    const char *green = "\033[32m";
    const char *red = "\033[31m";
    const char *dim = "\033[2m";
    const char *rst = "\033[0m";
    const char *symCol = success ? green : red;
    const char *sym = success ? "✓" : "✗";
    std::cout << grey << "⏱  " << t << " " << symCol << sym << grey << " " << dim << "[runtime]" << rst << "\n";
  } else {
    std::cout << "⏱  " << t << (success ? " ok" : " err") << " [runtime]\n";
  }
  std::cout << std::flush;
}

void Session::printHighlightedEcho(const std::string &code) const {
#ifdef _WIN32
  bool isTTY = _isatty(_fileno(stdout)) != 0;
#else
  bool isTTY = isatty(STDOUT_FILENO) != 0;
#endif
  if (!isTTY) return;
  bool color = shouldUseColor(false);
  if (!color) return;
  // Trim and limit to single-line preview (80 cols) for non-intrusive echo
  std::string preview = code;
  // Remove trailing newlines/spaces
  while (!preview.empty() && (preview.back()=='\n' || preview.back()=='\r' || preview.back()==' ' || preview.back()=='\t')) preview.pop_back();
  // Collapse internal newlines to " ⏎ " for preview
  for (char &c : preview) if (c=='\n' || c=='\r') c=' ';
  // Trim leading spaces
  size_t s = preview.find_first_not_of(" \t");
  if (s!=std::string::npos) preview = preview.substr(s);
  if (preview.empty()) return;
  if (preview.size() > 120) preview = preview.substr(0, 117) + "...";
  std::string highlighted = utils::Highlighter::highlight(preview, true);
  // Print with label [code] for clarity after execution (helps distinguish input echo from errors)
  if (color) {
    std::cout << "\033[90m  \u25B8 \033[0m" << highlighted << " \033[2;90m[code]\033[0m\n" << std::flush;
  } else {
    std::cout << "  > " << highlighted << " [code]\n" << std::flush;
  }
}

bool Session::isIncomplete(const std::string &buffer) const {
  return utils::IncompleteDetector::isIncomplete(buffer);
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
  if (t == ":reset" || t == ":flush" || t == ":forget" || t == ":clearstack" || t == ":drop") {
    // :flush is the explicit "flush the stack" requested by users — clears all definitions
    // so that already-defined variables no longer exist and can be redefined.
    // Aliases: :forget, :clearstack, :drop all map to the same full reset for now.
    // Future: :forget <var> could drop a single variable via Interpreter::forget().
    std::string target;
    if (t.rfind(":forget", 0) == 0) target = trim(t.substr(7));
    else if (t.rfind(":flush", 0) == 0) target = trim(t.substr(6));
    else if (t.rfind(":clearstack", 0) == 0) target = trim(t.substr(11));
    else if (t.rfind(":drop", 0) == 0) target = trim(t.substr(5));

    // If a specific variable name is given, try to forget just that variable
    if (!target.empty() && target[0] != ':' && target.find(' ') == std::string::npos && target.find('\t') == std::string::npos) {
      // Single word argument like ":flush x" or ":forget myVar"
      // For now, treat as full flush with hint (per-variable forgetting needs PTU tracking)
      // We still do a full reset but tell the user what was requested
      interp_.reset(err);
      if (!err.empty())
        std::cerr << "flush error: " << err << "\n";
      else
        std::cout << "[flushed stack" << (target.empty() ? "" : std::string(" (") + target + ")") << " — all definitions cleared, variables no longer exist]\n";
      promptCount_ = 1;
      hasLastTiming_ = false;
      return true;
    }
    if (!target.empty()) {
      std::cout << "usage: :flush [var]  (flush stack — clears all definitions so variables can be redefined)\n";
      std::cout << "       aliases: :forget, :clearstack, :drop (same as :reset)\n";
      return true;
    }
    interp_.reset(err);
    if (!err.empty())
      std::cerr << "reset error: " << err << "\n";
    else
      std::cout << (t.rfind(":flush",0)==0 || t.rfind(":forget",0)==0 || t.rfind(":clearstack",0)==0 || t.rfind(":drop",0)==0 ? "[flushed stack — all definitions cleared, variables no longer exist]\n" : "[reset]\n");
    promptCount_ = 1;
    hasLastTiming_ = false;
    return true;
  }
  if (t == ":clear" || t == ":cls" || t == ":c" || t == "clear" || t == "cls") {
#ifdef HAS_READLINE
#ifdef _WIN32
    if (_isatty(_fileno(stdin)) || _isatty(_fileno(stdout))) {
#else
    if (isatty(STDOUT_FILENO) || isatty(STDIN_FILENO)) {
#endif
      rl_clear_screen(0, 0);
      rl_on_new_line();
    }
#endif
#ifdef _WIN32
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
    {
      bool isTTY = isatty(STDOUT_FILENO) || isatty(STDERR_FILENO) || isatty(STDIN_FILENO);
      if (!isTTY) {
        std::cout << std::string(100, '\n') << std::flush;
      } else {
        const char *term = std::getenv("TERM");
        std::string termStr = term ? term : "";
        if (termStr.empty() || termStr == "dumb") {
          std::cout << std::string(100, '\n') << std::flush;
        } else {
          int rc = std::system("clear");
          if (rc != 0) {
            std::cout << "\033[2J\033[3J\033[H" << std::flush;
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
    else {
      std::cout << "[undid " << n << "]\n";
      if (promptCount_ > (int)n + 1) promptCount_ -= n;
      else promptCount_ = 1;
      hasLastTiming_ = false;
    }
    return true;
  }
  if (t.rfind(":I", 0) == 0 || t.rfind(":include", 0) == 0 || t.rfind(":inc", 0) == 0) {
    std::string path;
    if (t.rfind(":I", 0) == 0) path = trim(t.substr(2));
    else if (t.rfind(":include", 0) == 0) path = trim(t.substr(8));
    else path = trim(t.substr(4));
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

  auto printError = [&](const std::string &msg) {
    bool color = shouldUseColor(false);
    // Ensure we start on a new line (prompt and Clang diagnostics may be on same line)
    std::cerr << "\n";
    if (color) {
      std::cerr << "\033[31m[error]\033[0m " << msg << "\n";
      // If msg contains hint, highlight it with [fix] label
      if (msg.find("[hint]") != std::string::npos) {
        std::cerr << "\033[33m[fix]\033[0m " << "see hint above \u2192 try :undo or :reset, or add missing ';' / header\n";
      }
    } else {
      std::cerr << "[error] " << msg << "\n";
      if (msg.find("[hint]") != std::string::npos) {
        std::cerr << "[fix] see hint above -> try :undo or :reset, or add missing ';' / header\n";
      }
    }
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
      std::string promptStr = buffer_.empty() ? buildPrimaryPrompt(true)
                                              : buildContinuationPrompt(true);
      raw = readline(promptStr.c_str());
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
      // Fix for FILE *file without ; at [1] + FILE *file; with ; at [2] being two declarations
      // If buffer_ is "FILE *file" without ; and line is "FILE *file;" with ; and same, replace instead of append
      {
        auto trim2 = [](std::string s) {
          size_t a = s.find_first_not_of(" \t\r\n");
          if (a == std::string::npos) return std::string();
          size_t b = s.find_last_not_of(" \t\r\n");
          return s.substr(a, b - a + 1);
        };
        std::string bufTrim = trim2(buffer_);
        std::string lineTrim = trim2(line);
        if (!bufTrim.empty() && !lineTrim.empty() && bufTrim.back() != ';' && lineTrim.back() == ';' && bufTrim + ";" == lineTrim) {
          buffer_ = line + "\n";
        } else {
          buffer_ += line + "\n";
        }
      }
      bool incomplete = false;
      if (!line.empty() && line.back() == '\\')
        incomplete = true;
      else if (isIncomplete(buffer_))
        incomplete = true;
      if (incomplete) {
        continue;
      }
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
      auto t0 = std::chrono::steady_clock::now();
      bool ok = interp_.eval(buffer_, err);
      auto t1 = std::chrono::steady_clock::now();
      double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
      lastDurationMs_ = ms;
      lastSuccess_ = ok;
      hasLastTiming_ = true;
      // Keyword highlight: echo executed code with syntax colors (after execution)
      printHighlightedEcho(buffer_);
      if (!ok) {
        if (!err.empty()) printError(err);
        printTimingLine(false, ms);
      } else {
        printTimingLine(true, ms);
      }
      ++promptCount_;
      buffer_.clear();
    }
    if (!histFile.empty()) write_history(histFile.c_str());
    return;
  }
#endif
  std::string line;
  std::cout << buildPrimaryPrompt(false) << std::flush;
  while (std::getline(std::cin, line)) {
    std::string t = trim(line);
    if (buffer_.empty()) {
      if (t == ":quit" || t == ":exit" || t == ":q" || t == ":quit()" ||
          t == "exit" || t == "quit")
        break;
      if (t.empty()) {
        std::cout << buildPrimaryPrompt(false) << std::flush;
        continue;
      }
      std::string err;
      if (handleCommand(line, err)) {
        std::cout << buildPrimaryPrompt(false) << std::flush;
        continue;
      }
    }
    {
      auto trim2 = [](std::string s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return std::string();
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
      };
      std::string bufTrim = trim2(buffer_);
      std::string lineTrim = trim2(line);
      if (!bufTrim.empty() && !lineTrim.empty() && bufTrim.back() != ';' && lineTrim.back() == ';' && bufTrim + ";" == lineTrim) {
        buffer_ = line + "\n";
      } else {
        buffer_ += line + "\n";
      }
    }
    bool incomplete = false;
    if (!line.empty() && line.back() == '\\')
      incomplete = true;
    else if (isIncomplete(buffer_))
      incomplete = true;
    if (incomplete) {
      std::cout << buildContinuationPrompt(false) << std::flush;
      continue;
    }
    std::string err;
    auto t0 = std::chrono::steady_clock::now();
    bool ok = interp_.eval(buffer_, err);
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    lastDurationMs_ = ms;
    lastSuccess_ = ok;
    hasLastTiming_ = true;
    printHighlightedEcho(buffer_);
    if (!ok) {
      if (!err.empty()) printError(err);
      printTimingLine(false, ms);
    } else {
      printTimingLine(true, ms);
    }
    ++promptCount_;
    buffer_.clear();
    std::cout << buildPrimaryPrompt(false) << std::flush;
  }
  std::cout << "\nbye\n";
}

} // namespace repl
} // namespace cpprepl
