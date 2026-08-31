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
};

Options parse(int argc, char **argv, std::string &err);
void printHelp(const char *prog);
void printVersion();

} // namespace cli
} // namespace cpprepl
