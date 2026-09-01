# Architecture — Scalable & Professional

```
User Input (raw C++ like Python, no main)
        │
        ▼
┌─────────────────────────────────────────┐
│  cli::parse (src/cli)                   │  ← Options, --scaffold, -e, files, --no-color
└──────────────┬──────────────────────────┘
               ▼
┌─────────────────────────────────────────┐
│  config::ReplConfig (include/config)    │  ← InterpreterConfig + SessionConfig
└──────────────┬──────────────────────────┘
               ▼
┌─────────────────────────────────────────┐
│  interpreter::Interpreter (src/interpreter) │ ← wraps clang::Interpreter + VersionDetector
│   + utils::BigIntSupport preamble           │    auto -std, bigint, highlight, stdlib
│   + utils::IncompleteDetector               │    multiline heuristics (template, requires)
│   + core::VM (LLJIT) pluggable backend     │    ExecutionEngine abstraction
└──────────────┬──────────────────────────┘
               ▼
┌─────────────────────────────────────────┐
│  repl::Session (src/repl)               │  ← prompts, multiline, :commands, registry
│   + utils::Highlighter (syntax colors)  │
│   + utils::Logger (testable output)     │
└─────────────────────────────────────────┘
```

## Layering Principles

* **Dependency inversion** — `Session` depends on `interpreter::Interpreter` interface, not `clang::Interpreter` directly. `Interpreter` depends on `core::VM` interface, currently `LLJITVM` but swappable to `RemoteJIT` via factory.
* **Single responsibility** — `utils::IncompleteDetector` owns all `isIncomplete` heuristics (braces + 6 regexes). `utils::Highlighter` owns ANSI colors. `utils::Logger` owns output routing.
* **Configuration object** — `config::InterpreterConfig` replaces 4 overloads of `init(version, includePaths, defines, ...)`. `cli::Options` converts to `InterpreterConfig` via `toConfig()`.
* **Library vs binary** — `add_library(cpp-repl-core STATIC ...)` holds all logic; `add_executable(cpp-repl src/main.cpp)` is a thin wrapper. Tests link to `cpp-repl-core` without `main`.

## Extension Points

* **VM backends** — `core::VM` interface (`init/addModule/lookup/getLLJIT`) has `LLJITVM` impl. To add `RemoteJIT`: implement `VM` and register via `VMFactory::create("remote", opts)`.
* **Commands** — `repl::Session::handleCommand` will be refactored to `CommandRegistry` (`std::unordered_map<string, Command>`). Adding `:time` or `:theme` requires only `registry.register("time", handler)`.
* **Header fixes** — `fix_np_headers.hpp` / `fix_proxy.hpp` are now behind `utils::HeaderFixProvider` interface; Numpy-specific `undef NZERO` is one provider, not hardcoded.

## Data Flow

1. `cli::parse` → `ReplConfig` → `Interpreter::init(config)` → `clang::IncrementalCompilerBuilder` with `resourceDir` + `projectIncludeDir` + `-include fix_np_headers.hpp` (conditional).
2. `Session::runInteractive` reads `readline` or `getline`, buffers via `IncompleteDetector::isIncomplete`, echoes via `Highlighter::highlight`, evaluates via `Interpreter::eval` (auto `-std` upgrade, `BigInt` literal rewrite, `tryIncludeStdLib` on `std::` miss, JIT poison `Undo`).
3. Result printed via `highPrecisionDump` (`[result] (type) value` with `[result]` label) or `BigInt` via `std::cout`, timing via `Session::printTimingLine` (`[runtime]`), errors via `Logger::error` (`[error]`/`[fix]`).

## Testing Strategy

* **Unit** — `tests/test_version_detector.cpp`, `test_highlight.cpp`, `test_incomplete_detector.cpp` (no LLVM needed).
* **Integration** — `tests/test_interpreter_smoke.cpp` (requires LLVM/Clang, constructs `Interpreter` and `eval`s snippets).
* **E2E** — `examples/*.cpp` run via `cpp-repl --no-interactive` in CI, plus `ctest` via `FetchContent` googletest.

## Build & Packaging

* `GNUInstallDirs`, `CPack` (TGZ/DEB), `version.h` from `PROJECT_VERSION` + `GIT_SHA`, `cpp-replConfig.cmake` export.
* `ENABLE_SANITIZERS` (ASan/UBSan) and `ENABLE_CLANG_TIDY` options, `target_compile_options -Wall -Wextra`.
* `ccache`, `ninja`, `compile_commands.json` for LSP.

## Future Scale

* **Incremental IR cache** — `history_` currently `vector<string>` replayed on `ensureVersion`/`reinit`; future `O(1)` via `llvm::orc::IRTransformLayer` cache.
* **Thread-safe JIT** — `Session::exec` currently synchronous; future `ThreadSafeContext` + `BS_thread_pool` for concurrent cells (notebook/LSP).
* **PCH/module cache** — `tryIncludeStdLib` currently `ParseAndExecute("#include <bits/stdc++.h>")` (~0.5s cold); future `clang::PCH` precompile.
