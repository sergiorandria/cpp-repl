# Contributing to cpp-repl

Thanks for your interest! This is a scalable, professional-grade C++ REPL.

## Architecture (scalable)

```
include/cpp-repl/{core,interpreter,repl,cli,utils,config,security}
src/{core,interpreter,repl,cli,utils,security}
tests/ (googletest) — lib cpp-repl-core is testable
```

* `config::InterpreterConfig` — single source for `init` (replaces 4 overloads)
* `utils::IncompleteDetector` — central `isIncomplete` (template/requires/*)
* `utils::Logger` / `utils::Result<T>` — testable output / error handling
* `core::VM` + `InterpreterFactory` — pluggable LLJIT/RemoteJIT
* `repl::CommandRegistry` — extensible `:stack`/`:heap`/`:trace` without touching `Session`

See `docs/architecture.md` and `Doxyfile` (`make docs`).

## Development

Requires LLVM 22 + Clang 22 (with `clangInterpreter`), Boost, GMP (optional), Readline.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build -j
./build/cpp-repl --help
./build/cpp-repl --no-interactive examples/hello.cpp
ctest --test-dir build --output-on-failure
./benchmark/bench_repl  # micro-benchmarks (eval latency)
```

Pre-commit (clang-format + trailing whitespace):

```bash
pip install pre-commit && pre-commit install
pre-commit run --all-files
```

## Code Style

```bash
clang-format-22 -i $(find include src examples tests benchmark -name '*.h' -o -name '*.cpp' -o -name '*.hpp')
clang-tidy -p build --config-file=.clang-tidy src/**/*.cpp
```

We use `SYSTEM` for LLVM/Clang headers to silence `-Wsign-conversion` in Release.

## Pull Requests

- Keep PRs small and focused.
- Describe the change and how you tested it.
- CI must pass (build + format).
- Update `README.md` if user-facing behavior changes.

## Commit Messages

Use conventional prefixes when possible: `feat:`, `fix:`, `docs:`, `chore:`, `ci:`.
