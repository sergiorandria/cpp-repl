// version_concept.cpp – C++20 concepts, auto-detects -std=c++20
// Run: ./build/cpp-repl examples/version_concept.cpp --no-interactive

#include <iostream>

template<typename T>
concept Addable = requires(T a, T b) { a + b; };

template<Addable T>
T add(T a, T b) { return a + b; }

std::cout << "Addable 2 + 3 = " << add(2, 3) << std::endl;
std::cout << "Addable 2.5 + 3.5 = " << add(2.5, 3.5) << std::endl;

// C++20 using enum, requires etc. will auto-switch to C++20
