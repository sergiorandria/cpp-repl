# cpp-repl

Low-level C++ REPL (like Python REPL) built on LLVM VM concept.

- **VM core**: `llvm::orc::LLJIT` – in-process JIT, no optimization initially
- **Frontend**: `clang::Interpreter` (incremental parser + ORC executor) for real C++ parsing
- **REPL**: incremental state, value printing, diagnostics, `:commands`

## Architecture

```
User Input (C++ code)
       │
       ▼
clang::Interpreter (IncrementalParser) ──► LLVM IR Module
       │
       ▼
llvm::orc::LLJIT (VM) ──► native execution via ExecutorProcessControl
       │
       ▼
Value printing + persistent state
```

No optimizations (`O0`) – correctness first.

## Build

Requires LLVM 22 + Clang 22 (with `clangInterpreter`).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/cpp-repl
```

## CLI

```bash
cpp-repl [options] [file ...]
  -h, --help           show help
  -v, --version        show version
  --no-scaffold        skip low-level VM scaffold demo
  --no-interactive     exit after file/-e execution (CI)
  -e <code>            execute C++ code and exit
  <file>               load file before REPL
```

Examples:

```bash
./build/cpp-repl                          # interactive REPL
./build/cpp-repl examples/hello.cpp       # load file then REPL
./build/cpp-repl --no-scaffold -e 'int x=5; x*2' --no-interactive
echo 'int x=42; x+1' | ./build/cpp-repl --no-scaffold
```

## REPL Commands

```
:help  :h               show help
:quit :exit :q          exit REPL
:dump                   dump accumulated inputs (history)
:reset                  reset interpreter state
:load <file>            load and execute file
:lib <path>             load dynamic library (via LoadDynamicLibrary)
:undo [n]               undo last n inputs
```

## Examples

See `examples/`:

- `hello.cpp` – basic I/O and value printing
- `functions.cpp` – incremental function definitions (persistence)
- `class.cpp` – structs, classes, templates

Load with `:load examples/hello.cpp` or `./build/cpp-repl examples/class.cpp`

## Low-level VM detail

`src/vm.{h,cpp}` is the raw `llvm::orc::LLJIT` wrapper – it manually builds LLVM IR
(`LLVMContext`, `Module`, `IRBuilder`) and JIT-executes without Clang.
`src/repl.{h,cpp}` builds on top via `clang::Interpreter` (which internally uses the same ORC JIT)
to provide real C++ parsing.

No optimizations: all builds are `-O0 -g -fno-exceptions -fno-rtti` to stay close to the VM.

## License

MIT
