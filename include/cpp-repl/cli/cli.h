#pragma once
#include <string>
#include <vector>

namespace cpprepl {
namespace cli {

struct Options {
  bool showScaffold = false;
  bool noInteractive = false;
  bool showHelp = false;
  bool showVersion = false;
  std::vector<std::string> execCodes;
  std::vector<std::string> files;
  // Include / library support (absolute & relative)
  std::vector<std::string> includePaths;   // -I
  std::vector<std::string> libraryPaths;   // -L
  std::vector<std::string> libraries;      // -l
  std::vector<std::string> defines;        // -D
  std::vector<std::string> extraArgs;      // -- extra clang args
};

Options parse(int argc, char **argv, std::string &err);
void printHelp(const char *prog);
void printVersion();

} // namespace cli
} // namespace cpprepl
