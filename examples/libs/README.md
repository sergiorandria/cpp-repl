# libs example

Build the shared lib:
```bash
g++ -fPIC -shared examples/libs/mylib.cpp -o /tmp/libmylib.so
# or
g++ -fPIC -shared examples/libs/mylib.cpp -o ./tmp_lib/libmylib.so
```

Then use:
```bash
./build/cpp-repl -L ./tmp_lib -l mylib -e 'extern "C" int mylib_add(int,int); mylib_add(2,3)'
./build/cpp-repl -l /tmp/libmylib.so -e 'extern "C" int mylib_add(int,int); mylib_add(2,3)'
```
