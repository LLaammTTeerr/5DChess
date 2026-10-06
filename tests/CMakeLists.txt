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
  property_test.cpp
  position_test.cpp
  notation_test.cpp
  savestore_test.cpp
  confirm_test.cpp
  selection_test.cpp
  layout_test.cpp
  view_test.cpp
  guide_test.cpp
  ai_test.cpp
  vsai_test.cpp
  puzzle_test.cpp)
target_link_libraries(chess_tests PRIVATE play_core ai_core chess_core doctest::doctest)
target_include_directories(chess_tests PRIVATE ${PROJECT_SOURCE_DIR}/src) # ai/TurnGen.h (internal) is tested directly
target_compile_definitions(chess_tests PRIVATE FDCHESS_POSITIONS_DIR="${PROJECT_SOURCE_DIR}/assets/positions"
  FDCHESS_UI_POSITIONS_DIR="${PROJECT_SOURCE_DIR}/tests/ui/positions"
  FDCHESS_AI_POSITIONS_DIR="${PROJECT_SOURCE_DIR}/tests/ai"
  FDCHESS_GUIDE_DIR="${PROJECT_SOURCE_DIR}/assets/guide"
  FDCHESS_PUZZLES_DIR="${PROJECT_SOURCE_DIR}/assets/puzzles")
target_compile_options(chess_tests PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)

add_test(NAME chess_tests COMMAND chess_tests)

# The exhaustive proof of every shipped puzzle (docs/PUZZLES.md): the stored solutions are legal and win, mates in 2 hold against every defence.
if(TARGET puzzle_check)
  add_test(NAME puzzle_check COMMAND puzzle_check ${PROJECT_SOURCE_DIR}/assets/puzzles)
endif()
