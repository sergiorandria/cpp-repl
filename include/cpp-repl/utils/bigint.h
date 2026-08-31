#pragma once
#include <string>

namespace cpprepl {
namespace utils {

// Big integer support – like Python's arbitrary large ints.
// Uses boost::multiprecision::cpp_int (header-only) and GMP if available.
// Scalable: header provides preamble to inject into REPL.
class BigIntSupport {
public:
  // Preamble injected at REPL init for transparent bigint support.
  // Users can then write: bigint x = 12345678901234567890_cpp_int; or use cpp_int directly.
  static const char *preamble();

  // Alternative GMP preamble (uses mpz_int which wraps libgmp)
  static const char *gmpPreamble();

  // Check if boost headers are available (always true in our build)
  static bool isAvailable();

  // Example large number for testing
  static std::string demoCode();
};

} // namespace utils
} // namespace cpprepl
