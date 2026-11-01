cmake_minimum_required(VERSION 3.14)
project(repr LANGUAGES CXX)

include(FetchContent)

# --- Dependency: char-classify ---
# repr.cc includes "chars_escape.hh", char-classify's escaping kernel (the length table, the
# measuring pass and the writer shared with escape), and "chars.hh" for chars::char_repr (the
# byte-to-hex writer). The headers live in the sibling char-classify library, pulled in here so
# the includes resolve. It is pinned to a commit with GIT_TAG, like the rest of the family.
FetchContent_Declare(
  char_classify
  GIT_REPOSITORY https://github.com/Kronuz/char-classify.git
  GIT_TAG        65c1ff5c4052d9108ea7630bc4327be236ec1bd8
)
FetchContent_MakeAvailable(char_classify)

# --- Library target ---
# repr is not header-only: repr.cc holds the entry point (the truncation window, quotes and the
# exact sizing), so it must be compiled and linked. The header alone (repr.hh) only declares
# the overloads.
add_library(repr repr.cc)
target_include_directories(repr PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(repr PUBLIC cxx_std_20)

# Link char-classify so its headers are on the include path. PUBLIC because repr.cc
# uses them; consumers don't need them themselves, but linking it PUBLIC keeps
# the include path consistent and harmless.
target_link_libraries(repr PUBLIC char_classify::char_classify)

# Alias so consumers can link `repr::repr` whether they add this via
# add_subdirectory or FetchContent.
add_library(repr::repr ALIAS repr)

# Only build the test and demo when this repo is the top-level project, so
# consumers (e.g. via FetchContent) don't build them.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(repr_test test/test.cc)
  target_link_libraries(repr_test PRIVATE repr)
  add_test(NAME repr COMMAND repr_test)

  # Differential test against the frozen pre-optimization implementation.
  add_executable(repr_diff_test test/diff_test.cc test/repr_old.cc)
  target_link_libraries(repr_diff_test PRIVATE repr)
  add_test(NAME repr_diff COMMAND repr_diff_test)

  # The same two tests with the portable block backend forced (CHARS_NO_SIMD); the default
  # one is NEON or SSE2, so this is the only run of the 8-byte SWAR path there.
  add_library(repr_swar STATIC repr.cc)
  target_include_directories(repr_swar PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
  target_compile_features(repr_swar PUBLIC cxx_std_20)
  target_link_libraries(repr_swar PUBLIC char_classify::char_classify)
  target_compile_definitions(repr_swar PRIVATE CHARS_NO_SIMD=1)

  add_executable(repr_test_swar test/test.cc)
  target_link_libraries(repr_test_swar PRIVATE repr_swar)
  add_test(NAME repr_swar COMMAND repr_test_swar)

  add_executable(repr_diff_test_swar test/diff_test.cc test/repr_old.cc)
  target_link_libraries(repr_diff_test_swar PRIVATE repr_swar)
  add_test(NAME repr_diff_swar COMMAND repr_diff_test_swar)

  # Benchmark (not run by ctest): repr against a hand-written loop, the frozen original and
  # absl::CEscape. Abseil is fetched at a pinned release for this target only and is never a
  # dependency of the library.
  option(REPR_BUILD_BENCH "Build bench/bench.cc (fetches abseil)" OFF)
  if(REPR_BUILD_BENCH)
    set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
    set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(abseil
      GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
      GIT_TAG        20260817.0)
    FetchContent_MakeAvailable(abseil)
    add_executable(repr_bench bench/bench.cc test/repr_old.cc)
    target_link_libraries(repr_bench PRIVATE repr absl::strings)
    target_compile_definitions(repr_bench PRIVATE HAVE_ABSL=1)
  endif()

  # A runnable tour of the library (not a test): build & run ./build/repr_demo
  add_executable(repr_demo examples/demo.cc)
  target_link_libraries(repr_demo PRIVATE repr)
endif()
