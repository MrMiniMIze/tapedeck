include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

add_executable(eib_tests
  test_smoke.cpp
  test_order_book.cpp
  test_itch_parser.cpp
  test_book_differential.cpp
  test_match_engine.cpp)
target_link_libraries(eib_tests PRIVATE eib_core Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(eib_tests)
