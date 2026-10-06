# Developer tools. They reuse the game sources (minus main.cpp); GAME_SOURCES comes from the parent scope.
#   theme_preview: renders a real game with a chosen piece theme and saves a screenshot.
#   ui_script:     runs the real app from a script with injected input and saves screenshots (tests/ui).
set(TOOL_SOURCES ${GAME_SOURCES})
list(REMOVE_ITEM TOOL_SOURCES ${PROJECT_SOURCE_DIR}/src/main.cpp)

foreach(tool theme_preview ui_script)
  add_executable(${tool} ${tool}.cpp ${TOOL_SOURCES})
  target_include_directories(${tool} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Audio
    ${PROJECT_SOURCE_DIR}/include/Render)
  target_compile_definitions(${tool} PRIVATE FDCHESS_VERSION="${PROJECT_VERSION}")
  target_link_libraries(${tool} PRIVATE chess_core raylib)
  if(APPLE)
    target_link_libraries(${tool} PRIVATE
      "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreAudio" "-framework CoreVideo")
  endif()
  set_target_properties(${tool} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_command(TARGET ${tool} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${tool}>/assets)
endforeach()
