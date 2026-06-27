cmake_minimum_required(VERSION 3.20)
project(exchange_in_a_box VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for clangd / tooling.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(EIB_SANITIZE "Enable AddressSanitizer + UBSan (GCC/Clang only)" OFF)
option(EIB_WERROR   "Treat warnings as errors" OFF)
option(EIB_FUZZ     "Build libFuzzer targets for the parser (clang only)" OFF)

# ---------------------------------------------------------------------------
# Core STATIC library: the FeedParser / BookBuilder live here. Consumers
# inherit our include path, warning flags, and (optional) sanitizer flags.
# ---------------------------------------------------------------------------
add_library(eib_core STATIC
  src/order_book.cpp
  src/itch_parser.cpp
  src/match_engine.cpp)
target_include_directories(eib_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(eib_core PUBLIC cxx_std_20)

find_package(Threads REQUIRED)
target_link_libraries(eib_core PUBLIC Threads::Threads)

# Warnings + (optional) sanitizers, propagated to anything that links eib_core.
if(MSVC)
  target_compile_options(eib_core PUBLIC /W4 /permissive-
    $<$<BOOL:${EIB_WERROR}>:/WX>)
else()
  target_compile_options(eib_core PUBLIC
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    $<$<BOOL:${EIB_WERROR}>:-Werror>)
  if(EIB_SANITIZE)
    target_compile_options(eib_core PUBLIC
      -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(eib_core PUBLIC -fsanitize=address,undefined)
  endif()
endif()

# Fuzzing build (clang only): instrument the core with coverage + ASan/UBSan so
# libFuzzer gets coverage feedback through the parser, then build the harness.
if(EIB_FUZZ AND NOT MSVC)
  target_compile_options(eib_core PUBLIC
    -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer -g)
  target_link_options(eib_core PUBLIC -fsanitize=address,undefined)
  add_subdirectory(fuzz)
endif()

add_subdirectory(apps)

enable_testing()
add_subdirectory(tests)
