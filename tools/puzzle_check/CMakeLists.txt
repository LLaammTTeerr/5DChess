# puzzle_check: proves the puzzles of assets/puzzles with the engine (docs/PUZZLES.md). No graphics dependency.
add_executable(puzzle_check puzzle_check.cpp)
target_link_libraries(puzzle_check PRIVATE play_core chess_core)
target_compile_options(puzzle_check PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
set_target_properties(puzzle_check PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
