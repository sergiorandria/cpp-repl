# Contributing to cpp-repl

Thanks for your interest!

## Development

Requires LLVM 22 + Clang 22 (with `clangInterpreter`).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/cpp-repl --help
./build/cpp-repl --no-interactive examples/hello.cpp
```

## Code Style

```bash
clang-format-22 -i $(find include src examples -name '*.h' -o -name '*.cpp')
```

## Pull Requests

- Keep PRs small and focused.
- Describe the change and how you tested it.
- CI must pass (build + format).
- Update `README.md` if user-facing behavior changes.

## Commit Messages

Use conventional prefixes when possible: `feat:`, `fix:`, `docs:`, `chore:`, `ci:`.
