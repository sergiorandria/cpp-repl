#pragma once
#include <string>

namespace cpprepl {
namespace utils {

/**
 * @file bigint.h
 * @brief Big integer support via boost::multiprecision.
 */

/**
 * @brief Provides BigInt preamble and helpers for the REPL.
 *
 * Injects boost::multiprecision headers so users can write cpp_int / bigint
 * directly without manual includes, similar to Python's big ints.
 */
class BigIntSupport {
public:
  /**
   * @brief Preamble with cpp_int and bigint alias.
   * @return C++ code injected at interpreter init.
   */
  static const char *preamble();

  /**
   * @brief GMP-specific preamble (mpz_int).
   * @return C++ code for GMP backend.
   */
  static const char *gmpPreamble();

  /**
   * @brief Check if boost headers are available.
   * @return true if usable.
   */
  static bool isAvailable();

  /**
   * @brief Demo code for documentation and tests.
   * @return Example snippet using big ints.
   */
  static std::string demoCode();
};

} // namespace utils
} // namespace cpprepl
