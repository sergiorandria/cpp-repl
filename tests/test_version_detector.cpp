#include "cpp-repl/utils/version_detector.h"

#include <gtest/gtest.h>

using cpprepl::utils::StdVersion;
using cpprepl::utils::VersionDetector;

TEST(VersionDetector, DefaultIsCpp17) {
  EXPECT_EQ(VersionDetector::detect("int x=42;"), StdVersion::Cpp17);
  EXPECT_EQ(VersionDetector::detect("auto y=5;"), StdVersion::Cpp17);
}

TEST(VersionDetector, DetectsCpp20) {
  EXPECT_EQ(VersionDetector::detect("template<typename T> concept C = true;"), StdVersion::Cpp20);
  EXPECT_EQ(VersionDetector::detect("concept Foo = requires(int x){ x+1; };"), StdVersion::Cpp20);
  EXPECT_EQ(VersionDetector::detect("auto f() { co_await coro; }"), StdVersion::Cpp20);
}

TEST(VersionDetector, DetectsCpp23) {
  EXPECT_EQ(VersionDetector::detect("import std;"), StdVersion::Cpp23);
  EXPECT_EQ(VersionDetector::detect("export module foo;"), StdVersion::Cpp23);
}
