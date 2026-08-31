#pragma once
#include <memory>
#include <string>
#include <vector>
#include "cpp-repl/utils/version_detector.h"

namespace clang {
class Interpreter;
}

namespace cpprepl {
namespace interpreter {

// Scalable interpreter wrapper around clang::Interpreter.
// Handles C++ version auto-detection and BigInt preamble injection.
class Interpreter {
public:
  Interpreter();
  ~Interpreter();

  Interpreter(const Interpreter &) = delete;
  Interpreter &operator=(const Interpreter &) = delete;

  // Init with explicit version and include/lib options (absolute & relative)
  bool init(utils::StdVersion version, const std::vector<std::string> &includePaths,
            const std::vector<std::string> &defines, std::string &err);
  bool init(utils::StdVersion version,
            const std::vector<std::string> &includePaths,
            const std::vector<std::string> &defines,
            const std::vector<std::string> &libraryPaths,
            const std::vector<std::string> &libraries,
            std::string &err);
  bool init(utils::StdVersion version, std::string &err);
  bool init(std::string &err) { return init(utils::StdVersion::Cpp17, err); }
  bool init(const std::vector<std::string> &includePaths,
            const std::vector<std::string> &defines, std::string &err) {
    return init(utils::StdVersion::Cpp17, includePaths, defines, err);
  }

  // Dynamic include/library handling (interactive :I, :L)
  bool addIncludePath(const std::string &path, std::string &err);
  bool addLibraryPath(const std::string &path, std::string &err);
  bool addLibrary(const std::string &lib, std::string &err);

  // Auto-detect version from code and re-init if needed (scalable)
  bool evalAuto(const std::string &code, std::string &err);

  // Core eval – python-like: no main needed, raw code
  bool eval(const std::string &code, std::string &err);
  bool eval(const std::string &code, std::string &err, bool &incomplete);

  bool loadFile(const std::string &path, std::string &err);
  bool loadLibrary(const std::string &path, std::string &err);
  bool undo(unsigned n, std::string &err);

  void dump() const;
  void reset(std::string &err);
  void help() const;

  size_t historySize() const { return history_.size(); }
  utils::StdVersion currentVersion() const { return currentVersion_; }

private:
  bool ensureVersion(utils::StdVersion needed, std::string &err);
  bool reinitWithCurrentOptions(std::string &err);
  std::unique_ptr<clang::Interpreter> interp_;
  bool initialized_ = false;
  std::vector<std::string> history_;
  utils::StdVersion currentVersion_ = utils::StdVersion::Cpp17;
  std::vector<std::string> includePaths_;
  std::vector<std::string> defines_;
  std::vector<std::string> libraryPaths_;
  std::vector<std::string> libraries_;
  // Storage for compiler args c_str() lifetime
  std::vector<std::string> compilerArgsStorage_;
};

} // namespace interpreter
} // namespace cpprepl
