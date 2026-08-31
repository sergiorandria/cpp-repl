cmake_minimum_required(VERSION 3.20)
project(cpp-repl LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Do not optimize – O0 as requested
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fno-exceptions -fno-rtti")
# LLVM built without exceptions/rtti, keep it simple

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")
message(STATUS "LLVM includes: ${LLVM_INCLUDE_DIRS}")

# Clang package on Arch has version mismatch (LLVM 22.1.3 vs Clang 22.1.8).
# Bypass strict find_package(Clang) which requires EXACT match.
# Instead locate clang manually via clang-cpp shared lib.
find_library(CLANG_CPP_LIB clang-cpp HINTS /usr/lib)
if(NOT CLANG_CPP_LIB)
  message(FATAL_ERROR "clang-cpp not found")
endif()
set(CLANG_INCLUDE_DIRS /usr/include)
message(STATUS "Using clang-cpp: ${CLANG_CPP_LIB}")
message(STATUS "Clang includes: ${CLANG_INCLUDE_DIRS}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS})

# LLVM libs we need – low level VM
# Arch ships single libLLVM-22.so, not split libs. Use that.
find_library(LLVM_SINGLE_LIB LLVM-22 HINTS /usr/lib /usr/lib/llvm)
if(LLVM_SINGLE_LIB)
  set(LLVM_LIBS ${LLVM_SINGLE_LIB})
else()
  llvm_map_components_to_libnames(LLVM_LIBS
    Core
    Support
    OrcJIT
    ExecutionEngine
    Native
    IRReader
    Passes
  )
endif()
message(STATUS "LLVM libs: ${LLVM_LIBS}")

# Scalable modular sources
add_executable(cpp-repl
  src/main.cpp
  # Legacy low-level (kept for compat, now wrappers)
  src/vm.cpp
  src/repl.cpp
  # New scalable architecture
  src/core/vm.cpp
  src/interpreter/interpreter.cpp
  src/utils/version_detector.cpp
  src/utils/bigint.cpp
  src/repl/session.cpp
  src/cli/cli.cpp
)

target_include_directories(cpp-repl PRIVATE
  include
  src
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)

# BigInt support: boost header-only + optional GMP
find_package(Boost QUIET)
if(Boost_FOUND)
  message(STATUS "Boost found: ${Boost_INCLUDE_DIRS}")
  target_include_directories(cpp-repl PRIVATE ${Boost_INCLUDE_DIRS})
endif()
find_library(GMP_LIB gmp)
find_library(GMPXX_LIB gmpxx)
if(GMP_LIB)
  message(STATUS "GMP found: ${GMP_LIB}")
  target_link_libraries(cpp-repl PRIVATE ${GMP_LIB})
  if(GMPXX_LIB)
    target_link_libraries(cpp-repl PRIVATE ${GMPXX_LIB})
  endif()
  target_compile_definitions(cpp-repl PRIVATE HAS_GMP=1)
endif()

# Link against clang-cpp (contains Interpreter) and LLVM
# clang-cpp is a single shared lib that includes all clang libs in LLVM 22
target_link_libraries(cpp-repl PRIVATE
  ${CLANG_CPP_LIB}
  ${LLVM_LIBS}
)

# LLVM 22 is built with -fno-exceptions -fno-rtti, ensure we match
target_compile_options(cpp-repl PRIVATE -fno-exceptions -fno-rtti)

# Needed for JIT: export dynamic symbols + threads
find_package(Threads REQUIRED)
target_link_libraries(cpp-repl PRIVATE Threads::Threads ${CMAKE_DL_LIBS})

# Ensure we can JIT – require -rdynamic on Linux
if(UNIX)
  target_link_options(cpp-repl PRIVATE -rdynamic)
endif()

# Installation
install(TARGETS cpp-repl DESTINATION bin)
