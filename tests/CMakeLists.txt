FetchContent_Declare(doctest
  URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.11.tar.gz
  URL_HASH SHA256=632ed2c05a7f53fa961381497bf8069093f0d6628c5f26286161fbd32a560186
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
FetchContent_MakeAvailable(doctest)

add_executable(chess_tests
  main.cpp
  engine_test.cpp
  rules_test.cpp
  search_test.cpp
  directed_test.cpp
  property_test.cpp)
target_link_libraries(chess_tests PRIVATE chess_core doctest::doctest)
target_compile_options(chess_tests PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)

add_test(NAME chess_tests COMMAND chess_tests)
