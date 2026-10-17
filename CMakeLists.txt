cmake_minimum_required(VERSION 3.14)
project(repr LANGUAGES CXX)

include(FetchContent)

# --- Dependency: char-classify ---
# repr.cc includes "chars.hh" for chars::char_repr (the byte-to-hex writer). That
# header lives in the sibling char-classify library, pulled in here so the include
# resolves. We track its tip with GIT_TAG main, like the rest of the family.
FetchContent_Declare(
  char_classify
  GIT_REPOSITORY https://github.com/Kronuz/char-classify.git
  GIT_TAG        306ecd96d55d47d9bd3167eaeb7877e40d0631ac
)
FetchContent_MakeAvailable(char_classify)

# --- Library target ---
# repr is not header-only: repr.cc carries the escaping loop, so it must be
# compiled and linked. The header alone (repr.hh) only declares the overloads.
add_library(repr repr.cc)
target_include_directories(repr PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(repr PUBLIC cxx_std_20)

# Link char-classify so "chars.hh" is on the include path. PUBLIC because repr.cc
# uses it; consumers don't need chars.hh themselves, but linking it PUBLIC keeps
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

  # A runnable tour of the library (not a test): build & run ./build/repr_demo
  add_executable(repr_demo examples/demo.cc)
  target_link_libraries(repr_demo PRIVATE repr)
endif()
