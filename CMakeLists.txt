cmake_minimum_required(VERSION 3.28)

project(5DChess
    VERSION 1.0.0
  DESCRIPTION "Multidimensional chess with timeline mechanics"
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(FDCHESS_BUILD_GAME "Build the raylib game executable" ON)
option(FDCHESS_BUILD_TESTS "Build the unit/property test suite" ON)
option(FDCHESS_BUILD_TOOLS "Build developer tools (theme_preview screenshot tool; not installed)" OFF)
option(FDCHESS_BUILD_REFCHECK "Build tools/refcheck (random-game dumper for differential testing against 5d-chess-js)" OFF)
option(FDCHESS_BUILD_AIBENCH "Build tools/ai_bench (AI speed and self-play benchmark)" OFF)
option(FDCHESS_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

if(FDCHESS_SANITIZE AND NOT MSVC AND NOT EMSCRIPTEN)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

include(FetchContent)

# --- Rules engine (no graphics dependency) -----------------------------------
add_library(chess_core STATIC src/chess.cpp src/engine/Position.cpp src/engine/GameCatalog.cpp
  src/engine/Notation.cpp)
target_include_directories(chess_core PUBLIC include)
# The engine uses assert() as its validation layer: keep it on in every config.
target_compile_options(chess_core PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
if(MSVC)
  target_compile_options(chess_core PRIVATE /W4)
else()
  target_compile_options(chess_core PRIVATE -Wall -Wextra)
endif()

# --- AI opponent engine (no graphics dependency; docs/AI.md) ------------------
add_library(ai_core STATIC src/ai/Eval.cpp src/ai/TurnGen.cpp src/ai/Search.cpp src/ai/Play.cpp)
target_link_libraries(ai_core PUBLIC chess_core)
target_include_directories(ai_core PRIVATE src)
if(MSVC)
  target_compile_options(ai_core PRIVATE /W4)
else()
  target_compile_options(ai_core PRIVATE -Wall -Wextra)
endif()

# --- Game-screen logic that needs no graphics (selection state machine, board layout, multiverse snapshot, settings file text) -----------------
add_library(play_core STATIC src/play/Selection.cpp src/play/Feedback.cpp src/play/BoardLayout.cpp src/play/MultiverseView.cpp src/play/VsAi.cpp
  src/services/SettingsFile.cpp src/services/SaveStore.cpp src/guide/Guide.cpp src/Render/PieceThemes.cpp src/services/SettingsApply.cpp
  src/puzzles/Puzzle.cpp src/puzzles/Solver.cpp src/puzzles/Progress.cpp)
target_link_libraries(play_core PUBLIC chess_core)
if(MSVC)
  target_compile_options(play_core PRIVATE /W4)
else()
  target_compile_options(play_core PRIVATE -Wall -Wextra)
endif()

# --- Game ---------------------------------------------------------------------
if(FDCHESS_BUILD_GAME)
  if(EMSCRIPTEN)
    # WebAssembly build: always build raylib from source for the Web platform.
    set(PLATFORM Web CACHE STRING "" FORCE)
    set(raylib_FOUND FALSE)
  else()
    find_package(raylib 5.0 QUIET)
  endif()
  if(NOT raylib_FOUND)
    message(STATUS "raylib not found on the system, fetching raylib 5.5")
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
    FetchContent_Declare(raylib
      URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
      URL_HASH SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
      EXCLUDE_FROM_ALL) # keeps raylib's own install() rules out of our package
    FetchContent_MakeAvailable(raylib)
  endif()

  file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/*.cpp)
  list(REMOVE_ITEM GAME_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/chess.cpp)
  list(FILTER GAME_SOURCES EXCLUDE REGEX "/src/engine/|/src/ai/") # engine and AI sources are part of chess_core / ai_core
  list(FILTER GAME_SOURCES EXCLUDE REGEX "/src/guide/")  # guide content (no graphics) is part of play_core
  list(FILTER GAME_SOURCES EXCLUDE REGEX "/src/puzzles/(Puzzle|Solver|Progress)\\.cpp") # puzzle data, proofs and progress text: part of play_core
  list(FILTER GAME_SOURCES EXCLUDE REGEX "/src/play/(Selection|Feedback|BoardLayout|MultiverseView|VsAi)\\.cpp|/src/services/(SettingsFile|SaveStore)\\.cpp|/src/Render/PieceThemes\\.cpp|/src/services/SettingsApply\\.cpp") # part of play_core

  add_executable(5dchess ${GAME_SOURCES})
  target_include_directories(5dchess PRIVATE
    include
    include/Audio
    include/Render)
  target_compile_definitions(5dchess PRIVATE FDCHESS_VERSION="${PROJECT_VERSION}")
  target_link_libraries(5dchess PRIVATE play_core ai_core chess_core raylib)
  if(APPLE)
    target_link_libraries(5dchess PRIVATE
      "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreAudio" "-framework CoreVideo")
  endif()

  if(EMSCRIPTEN)
    # Web build: embed exactly what assets/manifest.txt lists (minus web=no entries) plus the manifest
    # itself. A "dir" entry embeds a whole directory and is skipped while it does not exist yet.
    set(MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/assets/manifest.txt)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${MANIFEST})
    set(WEB_ASSETS manifest.txt)
    file(STRINGS ${MANIFEST} MANIFEST_LINES)
    foreach(line ${MANIFEST_LINES})
      string(STRIP "${line}" line)
      if(line STREQUAL "" OR line MATCHES "^#")
        continue()
      endif()
      string(REGEX REPLACE "[ \t]+" ";" fields "${line}")
      list(GET fields 0 kind)
      list(GET fields 2 path)
      if(line MATCHES "[ \t]web=no([ \t]|$)")
        continue()
      endif()
      if(kind STREQUAL "dir" AND NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets/${path})
        continue()
      endif()
      list(APPEND WEB_ASSETS ${path})
    endforeach()
    set(WEB_LINK_OPTIONS -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1
      # raylib's bundled miniaudio ScriptProcessor callback reads Module.HEAPF32 (not exported by default)
      -sEXPORTED_RUNTIME_METHODS=HEAPF32
      "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
    foreach(a ${WEB_ASSETS})
      string(REGEX REPLACE "/$" "" a "${a}")
      list(APPEND WEB_LINK_OPTIONS "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets/${a}@/assets/${a}")
    endforeach()
    target_link_options(5dchess PRIVATE ${WEB_LINK_OPTIONS})
    set_target_properties(5dchess PROPERTIES SUFFIX ".html")
    # A "dir" preload has no dependency on the files inside it: relink when a position file changes.
    file(GLOB POSITION_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/positions/*.5dp)
    file(GLOB GUIDE_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/guide/*.5dp)
    file(GLOB PUZZLE_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/puzzles/*.5dp)
    set_property(TARGET 5dchess APPEND PROPERTY LINK_DEPENDS
      ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html ${MANIFEST} ${POSITION_FILES} ${GUIDE_FILES} ${PUZZLE_FILES})
  else()
    # Copy assets next to the binary so it runs from the build directory.
    add_custom_command(TARGET 5dchess POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:5dchess>/assets)

    install(TARGETS 5dchess RUNTIME DESTINATION .)
    install(DIRECTORY assets DESTINATION .)
    install(DIRECTORY docs/screenshots DESTINATION docs)
    install(FILES README.md LICENSE DESTINATION . OPTIONAL)

    set(CPACK_PACKAGE_NAME 5DChess)
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_GENERATOR ZIP)
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    include(CPack)
  endif()
endif()

# --- Developer tools (optional, never installed/packaged) ----------------------
if(FDCHESS_BUILD_TOOLS AND FDCHESS_BUILD_GAME AND NOT EMSCRIPTEN)
  add_subdirectory(tools)
endif()

# The puzzle prover has no graphics dependency: it is built with the tests (their ctest runs it) and with the tools.
if((FDCHESS_BUILD_TOOLS OR FDCHESS_BUILD_TESTS) AND NOT EMSCRIPTEN)
  add_subdirectory(tools/puzzle_check)
endif()

if(FDCHESS_BUILD_REFCHECK AND NOT EMSCRIPTEN)
  add_subdirectory(tools/refcheck)
endif()

if(FDCHESS_BUILD_AIBENCH AND NOT EMSCRIPTEN)
  add_subdirectory(tools/ai_bench)
endif()

# --- Tests --------------------------------------------------------------------
if(FDCHESS_BUILD_TESTS AND NOT EMSCRIPTEN)
  enable_testing()
  add_subdirectory(tests)
endif()
