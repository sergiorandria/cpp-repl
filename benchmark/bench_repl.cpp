/**
 * @file bench_repl.cpp
 * @brief Micro-benchmarks for the REPL (eval latency, history replay).
 */
#include "cpp-repl/interpreter/interpreter.h"
#include "cpp-repl/utils/version_detector.h"
#include <chrono>
#include <iostream>

int main() {
  using namespace cpprepl;
  interpreter::Interpreter interp;
  std::string err;
  if (!interp.init(utils::StdVersion::Cpp23, err)) {
    std::cerr << "init failed: " << err << "\n";
    return 1;
  }
  auto bench = [&](const std::string &code, const char* name) {
    auto t0 = std::chrono::steady_clock::now();
    for (int i=0;i<100;++i) {
      std::string e;
      interp.eval(code, e);
    }
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / 100.0;
    std::cout << name << ": " << ms << " ms / eval\n";
  };
  bench("int x = 42;", "int decl");
  bench("x + 1", "expr");
  bench("std::vector<int> v{1,2,3}; v.size();", "std::vector");
  // History replay bench
  auto t0 = std::chrono::steady_clock::now();
  {
    std::string e;
    interp.reset(e);
  }
  auto t1 = std::chrono::steady_clock::now();
  std::cout << "reset: " << std::chrono::duration<double, std::milli>(t1 - t0).count() << " ms\n";
  return 0;
}
