#pragma once

/**
 * @file config.h
 * @brief config — config module.
 */
#include "cpp-repl/security/sandbox.h"
#include "cpp-repl/utils/version_detector.h"

#include <string>
#include <vector>

namespace cpprepl {
namespace config {

// Centralized interpreter configuration (replaces 4 overloads + duplicated cli::Options fields)
struct InterpreterConfig {
  utils::StdVersion stdVersion = utils::StdVersion::Cpp23;
  std::vector<std::string> includePaths;
  std::vector<std::string> defines;
  std::vector<std::string> libraryPaths;
  std::vector<std::string> libraries;
  std::string resourceDir;
  std::string projectIncludeDir;
  bool enableBigInt = true;
  bool enableStdLib = true;
  bool enableGMP = false;
  security::SecurityConfig security;
};

struct SessionConfig {
  bool useColor = true;
  bool showTiming = true;
  bool highlightEcho = true;
  size_t previewMaxChars = 120;
};

struct ReplConfig {
  InterpreterConfig interpreter;
  SessionConfig session;
  bool interactive = true;
  bool showScaffold = false;
};

} // namespace config
} // namespace cpprepl