# ai_bench: speed of the AI per level and self-play between levels (Release build; see docs/AI.md).
add_executable(ai_bench ai_bench.cpp)
target_link_libraries(ai_bench PRIVATE ai_core chess_core)
target_compile_definitions(ai_bench PRIVATE FDCHESS_POSITIONS_DIR="${PROJECT_SOURCE_DIR}/assets/positions")
target_compile_options(ai_bench PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/W4,-Wall -Wextra>)
