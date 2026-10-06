# refcheck: differential-testing helper against 5d-chess-js (see compare.js). Not part of the game or the tests.
add_executable(refcheck refcheck.cpp)
target_link_libraries(refcheck PRIVATE chess_core)
target_compile_definitions(refcheck PRIVATE FDCHESS_POSITIONS_DIR="${PROJECT_SOURCE_DIR}/assets/positions")
target_compile_options(refcheck PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/W4,-Wall -Wextra>)

# turnbench: timing of the legal-turn search (random games over all modes, constructed mates). Use a Release build.
add_executable(turnbench turnbench.cpp)
target_link_libraries(turnbench PRIVATE chess_core)
