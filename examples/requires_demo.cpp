// requires_demo.cpp – C++20 requires / concept
template<typename T>
concept Incrementable = requires(T x) { ++x; x++; };

Incrementable auto inc(auto x) { return ++x; }

auto a = inc(41);
a
// Should print 42, auto-detects C++20
