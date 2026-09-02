#include "cpp-repl/utils/incomplete_detector.h"

#include <regex>

namespace cpprepl {
namespace utils {

bool IncompleteDetector::hasUnclosedBrace(const std::string &buffer) {
  int braces = 0, parens = 0, brackets = 0;
  bool inSingle = false, inDouble = false, inLineComment = false, inBlockComment = false;
  bool escaped = false;
  for (size_t i = 0; i < buffer.size(); ++i) {
    char c = buffer[i];
    char n = (i + 1 < buffer.size()) ? buffer[i + 1] : '\0';
    if (inLineComment) {
      if (c == '\n')
        inLineComment = false;
      continue;
    }
    if (inBlockComment) {
      if (c == '*' && n == '/') {
        inBlockComment = false;
        ++i;
      }
      continue;
    }
    if (inSingle) {
      if (escaped)
        escaped = false;
      else if (c == '\\')
        escaped = true;
      else if (c == '\'')
        inSingle = false;
      continue;
    }
    if (inDouble) {
      if (escaped)
        escaped = false;
      else if (c == '\\')
        escaped = true;
      else if (c == '"')
        inDouble = false;
      continue;
    }
    if (c == '/' && n == '/') {
      inLineComment = true;
      ++i;
      continue;
    }
    if (c == '/' && n == '*') {
      inBlockComment = true;
      ++i;
      continue;
    }
    if (c == '\'') {
      inSingle = true;
      continue;
    }
    if (c == '"') {
      inDouble = true;
      continue;
    }
    if (c == '{')
      ++braces;
    else if (c == '}')
      --braces;
    else if (c == '(')
      ++parens;
    else if (c == ')')
      --parens;
    else if (c == '[')
      ++brackets;
    else if (c == ']')
      --brackets;
  }
  if (inDouble || inSingle || inBlockComment)
    return true;
  return braces > 0 || parens > 0 || brackets > 0;
}

bool IncompleteDetector::isIncomplete(const std::string &buffer) {
  if (hasUnclosedBrace(buffer))
    return true;
  auto trimCopy = [](const std::string &s) -> std::string {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
      return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
  };
  std::string t = trimCopy(buffer);
  if (t.empty())
    return false;
  char last = t.back();
  if (last == ';' || last == '}')
    return false;
  if (last == ':' || last == '=' || last == ',')
    return true;
  if ((t.find('*') != std::string::npos || t.find('&') != std::string::npos) && last != ';' &&
      last != '}' && last != '{') {
    static const std::regex rePtrDecl(R"(.*\b\w+\s*[\*\&]+\s*\w+\s*$)");
    if (std::regex_match(t, rePtrDecl))
      return true;
  }
  {
    size_t posTpl = t.rfind("template");
    if (posTpl != std::string::npos) {
      size_t lastSemi = t.rfind(';');
      size_t lastRCurly = t.rfind('}');
      bool hasTermAfter = false;
      if (lastSemi != std::string::npos && lastSemi > posTpl)
        hasTermAfter = true;
      if (lastRCurly != std::string::npos && lastRCurly > posTpl)
        hasTermAfter = true;
      if (!hasTermAfter) {
        static const std::regex reTplHeader(R"(.*\btemplate\s*(<[^>]*>)?\s*$)",
                                            std::regex::ECMAScript);
        std::smatch m;
        if (std::regex_match(t, m, reTplHeader))
          return true;
        std::string afterTpl = t.substr(posTpl);
        if (afterTpl.find(';') == std::string::npos && afterTpl.find('{') == std::string::npos)
          return true;
        if (afterTpl.find('(') != std::string::npos && afterTpl.find(';') == std::string::npos &&
            afterTpl.find('{') == std::string::npos)
          return true;
      }
    }
  }
  {
    static const std::regex reStruct(R"(.*\b(struct|class|enum)\s+\w+(\s*:\s*[\w:,\s]+)?\s*$)");
    if (std::regex_match(t, reStruct))
      return true;
  }
  {
    static const std::regex reConcept(R"(.*\bconcept\s+\w+\s*=\s*.*)");
    if (std::regex_search(t, reConcept) && last != ';')
      return true;
    if (t.size() >= 8 && t.compare(t.size() - 8, 8, "requires") == 0)
      return true;
    static const std::regex reRequires(R"(.*\brequires\b[^;]*$)");
    if (std::regex_match(t, reRequires))
      return true;
  }
  {
    static const std::regex reCtrl(R"(.*\b(if|for|while|switch)\s*\(.*\)\s*$)");
    if (std::regex_match(t, reCtrl))
      return true;
  }
  {
    static const std::regex reTrailingKw(
        R"(.*\b(template|typename|concept|requires|struct|class|enum|public|private|protected)\s*$)");
    if (std::regex_match(t, reTrailingKw))
      return true;
  }
  return false;
}

} // namespace utils
} // namespace cpprepl
