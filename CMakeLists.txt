cmake_minimum_required(VERSION 3.14)
project(located_exception LANGUAGES CXX)

add_library(located_exception exception.cc)
target_include_directories(located_exception PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# C++20 so message formatting uses std::format with no third-party dependency.
# The standard is PUBLIC so consumers compile with it too, which keeps the same
# formatting backend (and therefore the same declared types in exception.h)
# selected on both sides.
target_compile_features(located_exception PUBLIC cxx_std_20)

# For a pre-C++20 toolchain without <format>, define WITHOUT_FORMAT (PUBLIC) to
# drop interpolation entirely (messages are stored as the raw format string).

# Build the smoke test only when this repo is the top-level project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(located_exception_test test/test.cc)
  target_link_libraries(located_exception_test PRIVATE located_exception)
  add_test(NAME located_exception COMMAND located_exception_test)
endif()
