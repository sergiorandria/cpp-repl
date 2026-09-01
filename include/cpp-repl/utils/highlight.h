#pragma once
#include <string>

namespace cpprepl {
namespace utils {

/**
 * @brief Simple C++ syntax highlighter for REPL output.
 *
 * Colors keywords, types, preprocessor, strings, comments, numbers.
 * Respects NO_COLOR / --no-color via shouldUseColor check externally,
 * but this function itself just checks useColor param.
 */
class Highlighter {
public:
  static std::string highlight(const std::string &code, bool useColor);
  static std::string highlightType(const std::string &typeStr, bool useColor);
  static std::string highlightValue(const std::string &valStr, bool useColor);
};

} // namespace utils
} // namespace cpprepl
