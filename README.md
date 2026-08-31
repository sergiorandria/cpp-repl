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

## REPL Commands

```
:help          show help
:quit / :exit  exit
:dump          dump accumulated IR
:reset         reset interpreter state
```

## License

MIT
