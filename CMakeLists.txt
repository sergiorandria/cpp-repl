cmake_minimum_required(VERSION 3.20)
project(cpp-repl VERSION 0.3.0 LANGUAGES C CXX)

# ── Language & standards ────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ── Options ─────────────────────────────────────────────────────────────
option(BUILD_TESTING "Build tests" ON)
option(ENABLE_SANITIZERS "Enable ASan/UBSan in Debug" OFF)
option(ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()
# Keep O0 as requested, but allow sanitizers to append
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# ── Version header ──────────────────────────────────────────────────────
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHA OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
else()
  set(GIT_SHA "unknown")
endif()
configure_file(include/cpp-repl/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/cpp-repl/version.h @ONLY)

# ── LLVM / Clang ────────────────────────────────────────────────────────
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")

find_library(CLANG_CPP_LIB NAMES clang-cpp clang-cpp-22
  HINTS /usr/lib /usr/lib/x86_64-linux-gnu /usr/lib/llvm-22/lib /usr/lib/llvm/lib
        ${LLVM_LIBRARY_DIRS} ${LLVM_LIBRARY_DIR} ${LLVM_DIR}/../.. ${LLVM_DIR}/../../..
  PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/lib/llvm-22/lib)
if(NOT CLANG_CPP_LIB)
  message(FATAL_ERROR "clang-cpp not found. HINTS: /usr/lib, /usr/lib/x86_64-linux-gnu, /usr/lib/llvm-22/lib")
endif()
find_path(CLANG_INCLUDE_DIRS NAMES clang/Basic/Version.h
  HINTS /usr/include /usr/lib/llvm-22/include /usr/lib/llvm/include ${LLVM_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS}/..
  PATHS /usr/include /usr/lib/llvm-22/include)
if(NOT CLANG_INCLUDE_DIRS)
  set(CLANG_INCLUDE_DIRS /usr/include)
endif()
message(STATUS "Using clang-cpp: ${CLANG_CPP_LIB}")
message(STATUS "Clang includes: ${CLANG_INCLUDE_DIRS}")

find_library(LLVM_SINGLE_LIB NAMES LLVM-22 LLVM
  HINTS /usr/lib /usr/lib/x86_64-linux-gnu /usr/lib/llvm-22/lib /usr/lib/llvm/lib
        ${LLVM_LIBRARY_DIRS} ${LLVM_LIBRARY_DIR} ${LLVM_DIR}/../.. ${LLVM_DIR}/../../..
  PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/lib/llvm-22/lib)
if(LLVM_SINGLE_LIB)
  set(LLVM_LIBS ${LLVM_SINGLE_LIB})
else()
  llvm_map_components_to_libnames(LLVM_LIBS Core Support OrcJIT ExecutionEngine Native IRReader Passes)
endif()

# ── Clang resource dir ─────────────────────────────────────────────────
set(_CLANG_FOR_RESOURCE "${CMAKE_CXX_COMPILER}")
if(_CLANG_FOR_RESOURCE)
  execute_process(COMMAND ${_CLANG_FOR_RESOURCE} -print-resource-dir
    OUTPUT_VARIABLE CLANG_RESOURCE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(NOT CLANG_RESOURCE_DIR OR CLANG_RESOURCE_DIR STREQUAL "")
  find_program(CLANG_DETECT NAMES clang-22 clang)
  if(CLANG_DETECT)
    execute_process(COMMAND ${CLANG_DETECT} -print-resource-dir
      OUTPUT_VARIABLE CLANG_RESOURCE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  endif()
endif()
if(NOT CLANG_RESOURCE_DIR OR CLANG_RESOURCE_DIR STREQUAL "")
  if(EXISTS "/usr/lib/clang/22")
    set(CLANG_RESOURCE_DIR "/usr/lib/clang/22")
  elseif(EXISTS "/usr/lib/llvm-22/lib/clang/22")
    set(CLANG_RESOURCE_DIR "/usr/lib/llvm-22/lib/clang/22")
  elseif(EXISTS "/usr/lib/llvm/lib/clang/22")
    set(CLANG_RESOURCE_DIR "/usr/lib/llvm/lib/clang/22")
  else()
    set(CLANG_RESOURCE_DIR "/usr/lib/clang/22")
  endif()
endif()
message(STATUS "Clang resource dir: ${CLANG_RESOURCE_DIR}")

# ── Core library (scalable, testable) ───────────────────────────────────
add_library(cpp-repl-core STATIC
  src/vm.cpp
  src/repl.cpp
  src/core/vm.cpp
  src/interpreter/interpreter.cpp
  src/utils/version_detector.cpp
  src/utils/bigint.cpp
  src/utils/highlight.cpp
  src/utils/incomplete_detector.cpp
  src/utils/variable_tracker.cpp
  src/security/sandbox.cpp
  src/repl/session.cpp
  src/repl/command_registry.cpp
  src/cli/cli.cpp
)
target_include_directories(cpp-repl-core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    src
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)
target_compile_definitions(cpp-repl-core
  PUBLIC ${LLVM_DEFINITIONS}
  PRIVATE
    CLANG_RESOURCE_DIR="${CLANG_RESOURCE_DIR}"
    CPP_REPL_INCLUDE_DIR="${PROJECT_SOURCE_DIR}/include"
)
target_compile_features(cpp-repl-core PUBLIC cxx_std_17)
target_compile_options(cpp-repl-core PRIVATE
  $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion>
)
if(ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy clang-tidy-22)
  if(CLANG_TIDY_EXE)
    set_target_properties(cpp-repl-core PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  endif()
endif()
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

# Dependencies for core
find_package(Boost QUIET)
if(Boost_FOUND)
  target_include_directories(cpp-repl-core PRIVATE ${Boost_INCLUDE_DIRS})
endif()
find_library(GMP_LIB gmp)
find_library(GMPXX_LIB gmpxx)
if(GMP_LIB)
  target_link_libraries(cpp-repl-core PRIVATE ${GMP_LIB})
  if(GMPXX_LIB)
    target_link_libraries(cpp-repl-core PRIVATE ${GMPXX_LIB})
  endif()
  target_compile_definitions(cpp-repl-core PRIVATE HAS_GMP=1)
endif()
find_library(READLINE_LIB readline)
find_library(HISTORY_LIB history)
find_path(READLINE_INCLUDE_DIR readline/readline.h)
if(READLINE_LIB AND READLINE_INCLUDE_DIR)
  target_include_directories(cpp-repl-core PRIVATE ${READLINE_INCLUDE_DIR})
  target_link_libraries(cpp-repl-core PRIVATE ${READLINE_LIB})
  if(HISTORY_LIB)
    target_link_libraries(cpp-repl-core PRIVATE ${HISTORY_LIB})
  endif()
  target_compile_definitions(cpp-repl-core PRIVATE HAS_READLINE=1)
endif()
find_package(Threads REQUIRED)
target_link_libraries(cpp-repl-core
  PUBLIC ${CLANG_CPP_LIB} ${LLVM_LIBS}
  PRIVATE Threads::Threads ${CMAKE_DL_LIBS}
)
if(UNIX)
  target_link_options(cpp-repl-core PRIVATE -rdynamic)
endif()

# ── Executable (thin wrapper) ───────────────────────────────────────────
add_executable(cpp-repl src/main.cpp)
target_link_libraries(cpp-repl PRIVATE cpp-repl-core)
target_compile_features(cpp-repl PRIVATE cxx_std_17)
if(UNIX)
  target_link_options(cpp-repl PRIVATE -rdynamic)
endif()

# ── Testing ─────────────────────────────────────────────────────────────
if(BUILD_TESTING)
  enable_testing()
  # Try system GTest first, fallback to FetchContent
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.14.0
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()
  add_subdirectory(tests)
endif()

# ── Install & Packaging ─────────────────────────────────────────────────
install(TARGETS cpp-repl cpp-repl-core
  EXPORT cpp-replTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/cpp-repl/version.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cpp-repl)
install(EXPORT cpp-replTargets
  NAMESPACE cpprepl::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-repl
)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cpp-replConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/cpp-replConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-repl
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/cpp-replConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/cpp-replConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cpp-replConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-repl
)

# CPack
set(CPACK_PACKAGE_NAME "cpp-repl")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "sergiorandria")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "C++ REPL like Python - LLVM JIT, C++17/20/23, BigInt")
set(CPACK_PACKAGE_CONTACT "sergiorandria")
set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libllvm22, libclang-cpp22, libreadline8, libboost-all-dev")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_SOURCE_IGNORE_FILES "/build/;/.git/;/.cache/;/.o/")
include(CPack)

# ── Doxygen (optional) ─────────────────────────────────────────────────
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
  add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating API docs with Doxygen"
  )
endif()
