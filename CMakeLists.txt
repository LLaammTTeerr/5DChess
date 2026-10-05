cmake_minimum_required(VERSION 3.28)

project(5DChess
  VERSION 0.2.0
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
option(FDCHESS_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

if(FDCHESS_SANITIZE AND NOT MSVC AND NOT EMSCRIPTEN)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

include(FetchContent)

# --- Rules engine (no graphics dependency) -----------------------------------
add_library(chess_core STATIC src/chess.cpp)
target_include_directories(chess_core PUBLIC include)
# The engine uses assert() as its validation layer: keep it on in every config.
target_compile_options(chess_core PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
if(MSVC)
  target_compile_options(chess_core PRIVATE /W4)
else()
  target_compile_options(chess_core PRIVATE -Wall -Wextra)
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

  add_executable(5dchess ${GAME_SOURCES})
  target_include_directories(5dchess PRIVATE
    include
    include/Audio
    include/Commands
    include/GameStates
    include/GameStates/ConcreteGameStates
    include/Menu
    include/Render
    include/Scene
    include/Scene/ConcreteScene)
  target_compile_definitions(5dchess PRIVATE FDCHESS_VERSION="${PROJECT_VERSION}")
  target_link_libraries(5dchess PRIVATE chess_core raylib)
  if(APPLE)
    target_link_libraries(5dchess PRIVATE
      "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreAudio" "-framework CoreVideo")
  endif()

  if(EMSCRIPTEN)
    # Web build: only the assets the game loads are embedded into the virtual
    # filesystem. assets/backgroundmusic is deliberately left out (size and
    # licensing), as are files the code never loads (buttons/, backgroundimage/,
    # unused fonts, per-theme cancel icons, ...).
    set(WEB_ASSETS
      chess.png ChessBoardNoBound.png images/EndGame.png
      fonts/PublicSans-Regular.ttf fonts/PublicSans-Bold.ttf fonts/Montserrat-Bold.ttf
      fonts/Nunito-VariableFont_wght.ttf fonts/intelone-mono-font-family-regular.ttf)
    foreach(theme 0 1 2 3)
      foreach(color black white)
        foreach(piece bishop king knight pawn queen rook)
          list(APPEND WEB_ASSETS images/Theme_${theme}/${color}_${piece}.png)
          if(theme EQUAL 3)  # eyes-closed blink frames of the Pixel theme
            list(APPEND WEB_ASSETS images/Theme_3/${color}_${piece}_blink.png)
          endif()
        endforeach()
      endforeach()
    endforeach()
    set(WEB_LINK_OPTIONS -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1
      "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
    foreach(a ${WEB_ASSETS})
      list(APPEND WEB_LINK_OPTIONS "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets/${a}@/assets/${a}")
    endforeach()
    target_link_options(5dchess PRIVATE ${WEB_LINK_OPTIONS})
    set_target_properties(5dchess PROPERTIES SUFFIX ".html")
    set_property(TARGET 5dchess APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)
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

# --- Tests --------------------------------------------------------------------
if(FDCHESS_BUILD_TESTS AND NOT EMSCRIPTEN)
  enable_testing()
  add_subdirectory(tests)
endif()
