# theme_preview: renders a real game with a chosen piece theme and saves a screenshot.
# Reuses the game sources (minus main.cpp); GAME_SOURCES comes from the parent scope.
set(PREVIEW_SOURCES ${GAME_SOURCES})
list(REMOVE_ITEM PREVIEW_SOURCES ${PROJECT_SOURCE_DIR}/src/main.cpp)

add_executable(theme_preview theme_preview.cpp ${PREVIEW_SOURCES})
target_include_directories(theme_preview PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/include/Commands
  ${PROJECT_SOURCE_DIR}/include/GameStates
  ${PROJECT_SOURCE_DIR}/include/GameStates/ConcreteGameStates
  ${PROJECT_SOURCE_DIR}/include/Menu
  ${PROJECT_SOURCE_DIR}/include/Render
  ${PROJECT_SOURCE_DIR}/include/Scene
  ${PROJECT_SOURCE_DIR}/include/Scene/ConcreteScene)
target_compile_definitions(theme_preview PRIVATE FDCHESS_VERSION="${PROJECT_VERSION}")
target_link_libraries(theme_preview PRIVATE chess_core raylib)
if(APPLE)
  target_link_libraries(theme_preview PRIVATE
    "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreAudio" "-framework CoreVideo")
endif()
set_target_properties(theme_preview PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_custom_command(TARGET theme_preview POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:theme_preview>/assets)
