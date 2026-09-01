#pragma once
#include <string>
#include <iostream>
#ifdef __has_include
#if __has_include(<unistd.h>)
#include <unistd.h>
#endif
#endif

namespace cpprepl {
namespace utils {

// Minimal logger abstraction - library never writes directly to cout/cerr
// Session/CLI decide where to route. Keeps core testable.
enum class Level { Debug, Info, Warn, Error };

class Logger {
public:
  explicit Logger(bool useColor = true) : useColor_(useColor) {}
  void setUseColor(bool v) { useColor_ = v; }
  void log(Level lvl, const std::string &msg) const {
    const char *prefix = "";
    const char *color = "";
    const char *rst = useColor_ ? "\033[0m" : "";
    switch (lvl) {
    case Level::Debug: prefix = "[debug] "; color = useColor_ ? "\033[90m" : ""; break;
    case Level::Info:  prefix = "[info] ";  color = useColor_ ? "\033[36m" : ""; break;
    case Level::Warn:  prefix = "[warn] ";  color = useColor_ ? "\033[33m" : ""; break;
    case Level::Error: prefix = "[error] "; color = useColor_ ? "\033[31m" : ""; break;
    }
    // Errors go to cerr, others to cout (keeps streams separable for tests)
    auto &os = (lvl == Level::Error || lvl == Level::Warn) ? std::cerr : std::cout;
    os << color << prefix << rst << msg << "\n";
  }
  void debug(const std::string &m) const { log(Level::Debug, m); }
  void info(const std::string &m) const { log(Level::Info, m); }
  void warn(const std::string &m) const { log(Level::Warn, m); }
  void error(const std::string &m) const { log(Level::Error, m); }

private:
  bool useColor_;
};

// Global helper to decide color (respects NO_COLOR)
inline bool shouldUseColor() {
  if (getenv("NO_COLOR") || getenv("CPP_REPL_NO_COLOR") || getenv("NO_COLOUR")) return false;
  if (getenv("FORCE_COLOR") || getenv("CLICOLOR_FORCE")) return true;
  const char *term = getenv("TERM");
  if (term && std::string(term) == "dumb") return false;
#ifdef _WIN32
  return false;
#else
  return isatty(STDOUT_FILENO) != 0;
#endif
}

} // namespace utils
} // namespace cpprepl
