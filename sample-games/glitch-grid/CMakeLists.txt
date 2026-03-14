find_package(SDL2 REQUIRED)

add_executable(glitchgrid
  main.c
)

target_link_libraries(glitchgrid
  PRIVATE
    pixelpal
    SDL2::SDL2
)

target_include_directories(glitchgrid
  PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(glitchgrid PRIVATE SDL_MAIN_HANDLED)

if(UNIX AND NOT APPLE)
  target_link_libraries(glitchgrid PRIVATE m)
endif()

pixelpal_copy_windows_runtime_dlls(glitchgrid)

if(WIN32)
  add_custom_command(
    TARGET glitchgrid
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/games/glitch-grid/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/manifest.toml"
      "${CMAKE_BINARY_DIR}/games/glitch-grid"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/icon.ppm"
      "${CMAKE_BINARY_DIR}/games/glitch-grid"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/splash.ppm"
      "${CMAKE_BINARY_DIR}/games/glitch-grid"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:glitchgrid>"
      "${CMAKE_BINARY_DIR}/games/glitch-grid/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:glitchgrid>/SDL2.dll"
      "${CMAKE_BINARY_DIR}/games/glitch-grid/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:glitchgrid>/libgcc_s_seh-1.dll"
      "${CMAKE_BINARY_DIR}/games/glitch-grid/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:glitchgrid>/libstdc++-6.dll"
      "${CMAKE_BINARY_DIR}/games/glitch-grid/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:glitchgrid>/libwinpthread-1.dll"
      "${CMAKE_BINARY_DIR}/games/glitch-grid/bin"
    VERBATIM
  )
endif()

install(TARGETS glitchgrid DESTINATION opt/pixelpal/games/glitch-grid/bin)
install(FILES
  manifest.toml
  icon.ppm
  splash.ppm
  DESTINATION opt/pixelpal/games/glitch-grid
)
