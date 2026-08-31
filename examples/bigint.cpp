// bigint.cpp – very large numbers like Python, no main needed
// Run: ./build/cpp-repl examples/bigint.cpp --no-interactive
// Or: :load examples/bigint.cpp

#include <iostream>

cpp_int a = cpp_int("1234567890123456789012345678901234567890");
cpp_int b = cpp_int("987654321098765432109876543210987654321");
cpp_int c = a * b;
std::cout << "a = " << a << "\n";
std::cout << "b = " << b << "\n";
std::cout << "a * b = " << c << "\n";

// bigint alias (same as cpp_int)
bigint x = cpp_int("999999999999999999999999999999");
bigint y = x * x;
std::cout << "x*x = " << y << "\n";

// GMP variant if available (requires -lgmp):
// #include <boost/multiprecision/gmp.hpp>
// mpz_int m = mpz_int("123456789012345678901234567890");
// std::cout << m * 10 << std::endl;
