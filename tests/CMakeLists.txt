add_executable(cpp-repl-tests
  test_version_detector.cpp
  test_highlight.cpp
  test_incomplete_detector.cpp
  test_interpreter_smoke.cpp
)

target_link_libraries(cpp-repl-tests PRIVATE cpp-repl-core GTest::gtest_main)
target_include_directories(cpp-repl-tests PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/generated)

include(GoogleTest)
gtest_discover_tests(cpp-repl-tests)
