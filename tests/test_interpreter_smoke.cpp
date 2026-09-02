#include "cpp-repl/interpreter/interpreter.h"
#include "cpp-repl/utils/version_detector.h"

#include <gtest/gtest.h>

using cpprepl::interpreter::Interpreter;
using cpprepl::utils::StdVersion;

TEST(InterpreterSmoke, InitAndEval) {
  Interpreter interp;
  std::string err;
  ASSERT_TRUE(interp.init(StdVersion::Cpp23, err)) << err;
  EXPECT_TRUE(interp.eval("int x = 42;", err)) << err;
  EXPECT_TRUE(interp.eval("x + 1", err)) << err;
}

TEST(InterpreterSmoke, StdLibAutoInclude) {
  Interpreter interp;
  std::string err;
  ASSERT_TRUE(interp.init(StdVersion::Cpp23, err)) << err;
  EXPECT_TRUE(interp.eval("std::vector<int> v{1,2,3}; v.size();", err)) << err;
}
