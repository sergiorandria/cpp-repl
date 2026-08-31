// Example lib for -L/-l demo: g++ -fPIC -shared examples/libs/mylib.cpp -o examples/libs/libmylib.so
extern "C" int mylib_add(int a, int b) { return a + b + 100; }
extern "C" int mylib_mul(int a, int b) { return a * b; }
