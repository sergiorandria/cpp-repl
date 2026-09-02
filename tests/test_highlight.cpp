#include "cpp-repl/utils/highlight.h"

#include <gtest/gtest.h>

using cpprepl::utils::Highlighter;

TEST(Highlighter, NoColorPassthrough) {
  EXPECT_EQ(Highlighter::highlight("int x=42;", false), "int x=42;");
}

TEST(Highlighter, ColorsKeywordsWhenEnabled) {
  auto s = Highlighter::highlight("int x=42;", true);
  EXPECT_NE(s, "int x=42;");
  EXPECT_NE(s.find("\033["), std::string::npos);
}

TEST(Highlighter, HighlightsPreprocessor) {
  auto s = Highlighter::highlight("#include <vector>", true);
  EXPECT_NE(s.find("\033["), std::string::npos);
}
