#pragma once

/**
 * @file incomplete_detector.h
 * @brief incomplete_detector — utils module.
 */
#include <string>

namespace cpprepl {
namespace utils {

// Centralized incomplete-input detection (extracted from repl/session + interpreter)
// Single source of truth for brace/paren/bracket + template/concept/requires heuristics.
class IncompleteDetector {
public:
  static bool isIncomplete(const std::string &buffer);
  // Exposed for testing
  static bool hasUnclosedBrace(const std::string &buffer);
};

} // namespace utils
} // namespace cpprepl