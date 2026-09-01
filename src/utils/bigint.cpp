#include "cpp-repl/utils/bigint.h"

namespace cpprepl {
namespace utils {

const char *BigIntSupport::preamble() {
  return R"CPP(
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
using boost::multiprecision::cpp_int;
using boost::multiprecision::cpp_dec_float_50;
using boost::multiprecision::cpp_bin_float_50;
using bigint = cpp_int;
using bigfloat = cpp_dec_float_50;
using cpp_bin_float = boost::multiprecision::cpp_bin_float_50;
// Python-like big ints/floats: e.g. cpp_int x = cpp_int("123..."); bigfloat y = bigfloat("3.14159...")
// Large literals like bigint g = 4949... (50 digits) are auto-wrapped to bigint("...")
// and bigfloat f = 3.14159... (30+ digits) to bigfloat("3.14159...")
)CPP";
}

const char *BigIntSupport::gmpPreamble() {
  return R"CPP(
#include <boost/multiprecision/gmp.hpp>
#include <boost/multiprecision/mpfr.hpp>
using boost::multiprecision::mpz_int;
using boost::multiprecision::mpq_rational;
using boost::multiprecision::mpfr_float_50;
using boost::multiprecision::mpfr_float_100;
using mpz = boost::multiprecision::mpz_int;
using mpq = boost::multiprecision::mpq_rational;
using bigfloat_mpfr = boost::multiprecision::mpfr_float_50;
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
