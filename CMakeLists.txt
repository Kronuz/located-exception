cmake_minimum_required(VERSION 3.14)
project(located_exception LANGUAGES CXX)

add_library(located_exception exception.cc)
target_include_directories(located_exception PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(located_exception PUBLIC cxx_std_17)

# Use {fmt} for format-string interpolation in THROW(...) if it's available;
# otherwise fall back to a zero-dependency, no-interpolation mode. The toggle is
# PUBLIC so it stays consistent across this library and every consumer (the
# declared types in exception.h depend on it).
find_package(fmt QUIET)
if(fmt_FOUND)
  target_link_libraries(located_exception PUBLIC fmt::fmt)
else()
  target_compile_definitions(located_exception PUBLIC WITHOUT_FMT)
endif()

# Build the smoke test only when this repo is the top-level project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(located_exception_test test/test.cc)
  target_link_libraries(located_exception_test PRIVATE located_exception)
  add_test(NAME located_exception COMMAND located_exception_test)
endif()
