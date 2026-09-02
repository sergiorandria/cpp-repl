#include "cpp-repl/utils/incomplete_detector.h"

#include <gtest/gtest.h>

using cpprepl::utils::IncompleteDetector;

TEST(IncompleteDetector, Braces) {
  EXPECT_TRUE(IncompleteDetector::isIncomplete("int foo() {"));
  EXPECT_FALSE(IncompleteDetector::isIncomplete("int foo() {}"));
}

TEST(IncompleteDetector, TemplateHeader) {
  EXPECT_TRUE(IncompleteDetector::isIncomplete("template <typename T>"));
  EXPECT_FALSE(IncompleteDetector::isIncomplete("template <typename T> struct Foo {};"));
}

TEST(IncompleteDetector, PointerWithoutSemi) {
  EXPECT_TRUE(IncompleteDetector::isIncomplete("FILE *file"));
  EXPECT_FALSE(IncompleteDetector::isIncomplete("FILE *file;"));
}

TEST(IncompleteDetector, Requires) {
  EXPECT_TRUE(IncompleteDetector::isIncomplete("requires std::is_integral_v<T>"));
}
