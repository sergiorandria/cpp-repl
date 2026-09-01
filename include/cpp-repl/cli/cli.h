#pragma once
#include <string>
#include <vector>

namespace cpprepl {
namespace cli {

/**
 * @file cli.h
 * @brief Command-line parsing for cpp-repl.
 */

/**
 * @brief Parsed command-line options.
 */
struct Options {
  bool showScaffold = false; ///< Show low-level VM IR demo.
  bool noInteractive = false; ///< Exit after -e/file execution.
  bool showHelp = false; ///< Print help.
  bool showVersion = false; ///< Print version.
  std::vector<std::string> execCodes; ///< Code from -e flags.
  std::vector<std::string> files; ///< Input files.
  std::vector<std::string> includePaths; ///< Include paths (-I).
  std::vector<std::string> libraryPaths; ///< Library search paths (-L).
  std::vector<std::string> libraries; ///< Libraries to link (-l).
  std::vector<std::string> defines; ///< Macro definitions (-D).
  std::vector<std::string> extraArgs; ///< Extra clang args.
};

/**
 * @brief Parse command-line arguments.
 * @param argc Argument count.
 * @param argv Argument values.
 * @param err Output error message.
 * @return Parsed options.
 */
Options parse(int argc, char **argv, std::string &err);

/**
 * @brief Print help to stdout.
 * @param prog Program name (argv[0]).
 */
void printHelp(const char *prog);

/**
 * @brief Print version to stdout.
 */
void printVersion();

} // namespace cli
} // namespace cpprepl
