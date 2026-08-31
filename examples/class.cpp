// class.cpp – C++ classes and incremental state
#include <iostream>
#include <string>

struct Counter {
  int value = 0;
  void inc() { ++value; }
  void dec() { --value; }
  int get() const { return value; }
};

Counter c;
c.inc();
c.inc();
std::cout << "Counter: " << c.get() << std::endl;

// Templates also work incrementally
template<typename T>
T square(T x) { return x*x; }

// square(5) -> 25, square(3.14) -> 9.859...
