/**
 * @file bigint.cpp
 * @brief BigInt preamble definitions.
 */
#include "cpp-repl/utils/bigint.h"

namespace cpprepl {
namespace utils {

const char *BigIntSupport::preamble() {
  return R"CPP(
#include <boost/multiprecision/cpp_int.hpp>
using boost::multiprecision::cpp_int;
using bigint = cpp_int;
// Python-like big ints: e.g. cpp_int x = cpp_int("123456789012345678901234567890");
// Large literals like bigint g = 4949... (50 digits) are auto-wrapped to bigint("...")
// Note: mpz/mpq aliases are provided by gmpPreamble if GMP is available
)CPP";
}

const char *BigIntSupport::gmpPreamble() {
  return R"CPP(
#include <boost/multiprecision/gmp.hpp>
using boost::multiprecision::mpz_int;
using boost::multiprecision::mpq_rational;
using mpz = boost::multiprecision::mpz_int;
using mpq = boost::multiprecision::mpq_rational;
// Provide bigint alias for compatibility (already defined, but ensure)
)CPP";
}

bool BigIntSupport::isAvailable() {
#ifdef __has_include
#if __has_include(<boost/multiprecision/cpp_int.hpp>)
  return true;
#else
  return false;
#endif
#else
  return true; // assume available, header-only
#endif
}

std::string BigIntSupport::demoCode() {
  return R"CPP(cpp_int a = cpp_int("1234567890123456789012345678901234567890");
cpp_int b = cpp_int("987654321098765432109876543210987654321");
cpp_int c = a * b;
c
)CPP";
}

} // namespace utils
} // namespace cpprepl
