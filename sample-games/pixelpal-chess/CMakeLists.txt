find_package(SDL2 REQUIRED)

add_executable(pixelpal-chess
  main.cpp
)

target_link_libraries(pixelpal-chess
  PRIVATE
    pixelpal
    SDL2::SDL2
)

target_include_directories(pixelpal-chess
  PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(pixelpal-chess PRIVATE SDL_MAIN_HANDLED)

if(UNIX AND NOT APPLE)
  target_link_libraries(pixelpal-chess PRIVATE m)
endif()

pixelpal_copy_windows_runtime_dlls(pixelpal-chess)

if(WIN32)
  add_custom_command(
    TARGET pixelpal-chess
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/games/pixelpal-chess/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/manifest.toml"
      "${CMAKE_BINARY_DIR}/games/pixelpal-chess"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/icon.ppm"
      "${CMAKE_BINARY_DIR}/games/pixelpal-chess"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/splash.ppm"
      "${CMAKE_BINARY_DIR}/games/pixelpal-chess"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:pixelpal-chess>"
      "${CMAKE_BINARY_DIR}/games/pixelpal-chess/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:pixelpal-chess>/SDL2.dll"
      "${CMAKE_BINARY_DIR}/games/pixelpal-chess/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:pixelpal-chess>/libgcc_s_seh-1.dll"
      "${CMAKE_BINARY_DIR}/games/pixelpal-chess/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:pixelpal-chess>/libstdc++-6.dll"
      "${CMAKE_BINARY_DIR}/games/pixelpal-chess/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:pixelpal-chess>/libwinpthread-1.dll"
      "${CMAKE_BINARY_DIR}/games/pixelpal-chess/bin"
    VERBATIM
  )
endif()

install(TARGETS pixelpal-chess DESTINATION opt/pixelpal/games/pixelpal-chess/bin)
install(FILES
  manifest.toml
  icon.ppm
  splash.ppm
  DESTINATION opt/pixelpal/games/pixelpal-chess
)
