#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "cpp-repl/utils/version_detector.h"

namespace clang {
class Interpreter;
}

namespace cpprepl {
namespace interpreter {

/**
 * @file interpreter.h
 * @brief Scalable interpreter wrapper around clang::Interpreter.
 */

/**
 * @brief High-level REPL interpreter with version and BigInt support.
 *
 * Wraps clang::Interpreter (incremental parsing + ORC JIT), auto-detects
 * C++ standard, injects BigInt preamble, and handles include/library paths.
 * Supports incremental eval without a main function, like Python REPL.
 */
class Interpreter {
public:
  Interpreter();
  ~Interpreter();

  Interpreter(const Interpreter &) = delete;
  Interpreter &operator=(const Interpreter &) = delete;

  /**
   * @brief Init with explicit version and include/lib options.
   * @param version Initial C++ standard.
   * @param includePaths Include search paths (-I).
   * @param defines Macro definitions (-D).
   * @param err Output error.
   * @return true on success.
   */
  bool init(utils::StdVersion version, const std::vector<std::string> &includePaths,
            const std::vector<std::string> &defines, std::string &err);
  /**
   * @brief Init with full options.
   * @param version Initial C++ standard.
   * @param includePaths Include paths.
   * @param defines Defines.
   * @param libraryPaths Library search paths (-L).
   * @param libraries Libraries to link (-l).
   * @param err Output error.
   * @return true on success.
   */
  bool init(utils::StdVersion version,
            const std::vector<std::string> &includePaths,
            const std::vector<std::string> &defines,
            const std::vector<std::string> &libraryPaths,
            const std::vector<std::string> &libraries,
            std::string &err);
  /** @brief Init with version only. */
  bool init(utils::StdVersion version, std::string &err);
  /** @brief Init with C++23 default. */
  bool init(std::string &err) { return init(utils::StdVersion::Cpp23, err); }
  /** @brief Init with includes/defines and C++23 default. */
  bool init(const std::vector<std::string> &includePaths,
            const std::vector<std::string> &defines, std::string &err) {
    return init(utils::StdVersion::Cpp23, includePaths, defines, err);
  }

  /**
   * @brief Add an include search path at runtime.
   * @param path Absolute or relative path.
   * @param err Output error.
   * @return true on success, re-inits and replays history.
   */
  bool addIncludePath(const std::string &path, std::string &err);
  /**
   * @brief Add a library search path at runtime.
   * @param path Absolute or relative path.
   * @param err Output error.
   * @return true on success.
   */
  bool addLibraryPath(const std::string &path, std::string &err);
  /**
   * @brief Load a library by name or path.
   * @param lib Library name or file path.
   * @param err Output error.
   * @return true on success.
   */
  bool addLibrary(const std::string &lib, std::string &err);

  /**
   * @brief Evaluate code with automatic version detection.
   * @param code Raw C++ code.
   * @param err Output error.
   * @return true on success.
   */
  bool evalAuto(const std::string &code, std::string &err);

  /**
   * @brief Evaluate raw C++ code incrementally.
   * @param code C++ source without main.
   * @param err Output error.
   * @return true on success.
   */
  bool eval(const std::string &code, std::string &err);
  /**
   * @brief Evaluate with incomplete-input detection.
   * @param code Code buffer.
   * @param err Output error.
   * @param incomplete Set to true if braces/parens are unbalanced.
   * @return true if handled.
   */
  bool eval(const std::string &code, std::string &err, bool &incomplete);

  /** @brief Load and execute a raw C++ file. */
  bool loadFile(const std::string &path, std::string &err);
  /** @brief Load a dynamic library. */
  bool loadLibrary(const std::string &path, std::string &err);
  /**
   * @brief Undo last N inputs.
   * @param n Number of PTUs to remove.
   * @param err Output error.
   * @return true on success.
   */
  bool undo(unsigned n, std::string &err);

  /** @brief Dump history and current version to stdout. */
  void dump() const;
  /** @brief Reset interpreter state. */
  void reset(std::string &err);
  /** @brief Print help text. */
  void help() const;

  /** @brief Number of history entries. */
  size_t historySize() const { return history_.size(); }
  /** @brief Current C++ standard. */
  utils::StdVersion currentVersion() const { return currentVersion_; }

private:
  /** @brief Ensure at least the needed C++ version, re-init if higher. */
  bool ensureVersion(utils::StdVersion needed, std::string &err);
  /** @brief Re-init with current options and replay history. */
  bool reinitWithCurrentOptions(std::string &err);
  /** @brief Sanitize include directives (e.g., strip trailing semicolon). */
  std::string sanitizeIncludes(const std::string &code);
  /** @brief Reject redefinition with different value. */
  bool checkVariableRedefinition(const std::string &code, std::string &err);
  /** @brief Track variable declarations for redefinition checks. */
  void trackVariable(const std::string &code);
  /** @brief Parse a declaration into type/name/value. */
  bool parseDeclaration(const std::string &code, std::string &type,
                        std::string &name, std::string &value);
  /** @brief Parse an assignment into name/value. */
  bool parseAssignment(const std::string &code, std::string &name,
                       std::string &value);
  /** @brief Normalize a value string for comparison. */
  std::string normalizeValue(const std::string &v);
  /** @brief Ensure standard library is available (bits/stdc++.h). */
  bool ensureStdLib(std::string &err);
  /** @brief Try to include bits/stdc++.h or fallback headers. */
  bool tryIncludeStdLib();
  std::unique_ptr<clang::Interpreter> interp_;
  bool initialized_ = false;
  std::vector<std::string> history_;
  utils::StdVersion currentVersion_ = utils::StdVersion::Cpp17;
  std::vector<std::string> includePaths_;
  std::vector<std::string> defines_;
  std::vector<std::string> libraryPaths_;
  std::vector<std::string> libraries_;
  std::vector<std::string> compilerArgsStorage_;
  std::unordered_map<std::string, std::pair<std::string, std::string>> variables_;
  std::vector<std::unordered_map<std::string, std::pair<std::string, std::string>>> varHistory_;
  bool stdLibIncluded_ = false;
};

} // namespace interpreter
} // namespace cpprepl
