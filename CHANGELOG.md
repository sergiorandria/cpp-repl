# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.3.0] - 2026-09-01

### Added
- Colored prompt `cpp:C++23 [n] (time ✓/✗)>` with version/counter/timing, `NO_COLOR`/`--no-color` support
- Syntax highlight after execution (`▸` with keyword/type/string colors) via `utils::Highlighter`
- Multiline definition support for `template <typename T>`, `requires`/`concept`, `struct`/`class`, and `*`/`&` without `;`
- Standard library auto-include (`<bits/stdc++.h>` by default, fallback bundle) and JIT poison recovery (`Undo` + hint)
- `parseDeclaration` now handles `FILE *file;` with `*`/`&` and direct init `b({0,2,4,6})` for `np::ndarray`
- Scalable architecture: `config::InterpreterConfig`, `utils::Logger`, `utils::IncompleteDetector` centralization
- `cpp-repl-core` static library for testability, `version.h` generation from `PROJECT_VERSION`
- `tests/` with GoogleTest (version_detector, highlight, incomplete_detector, interpreter_smoke)
- `CPack` DEB/TGZ packaging, `GNUInstallDirs`, `cpp-replConfig.cmake` export
- `.clang-tidy`, `Doxyfile`, `CHANGELOG.md`

### Fixed
- Conditional `-include` for `fix_np_headers.hpp` to avoid `fatal error: 'cpp-repl/fix_np_headers.hpp' file not found` in CI artifact
- `FILE *file;` ambiguity: `FILE *file` without `;` now correctly buffered as `...>` and `FILE *file;` duplicate same-type now `[ignored]` instead of `redefinition`
- `np::ndarray` direct init `b({0,2,4,6})` now correctly parsed

### Changed
- CMake modernized: `target_*` instead of global `include_directories`, `GNUInstallDirs`, `BUILD_TESTING`, `ENABLE_SANITIZERS`, `ENABLE_CLANG_TIDY`
- `Session::isIncomplete` and `Interpreter::eval(..., incomplete)` now delegate to `IncompleteDetector`

## [0.2.0] - 2024-12-01
- Initial LLVM 22/Clang 22 support, BigInt, auto `-std` detection, `LLJIT` VM

## [0.1.0] - 2024-11-15
- Initial release with `clang::Interpreter` + `LLJIT`
