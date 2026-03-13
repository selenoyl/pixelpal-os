find_package(SDL2 REQUIRED)

add_executable(botbyte
  main.c
)

target_link_libraries(botbyte
  PRIVATE
    pixelpal
    SDL2::SDL2
)

target_include_directories(botbyte
  PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(botbyte PRIVATE SDL_MAIN_HANDLED)

if(UNIX AND NOT APPLE)
  target_link_libraries(botbyte PRIVATE m)
endif()

pixelpal_copy_windows_runtime_dlls(botbyte)

if(WIN32)
  add_custom_command(
    TARGET botbyte
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/games/botbyte/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/manifest.toml"
      "${CMAKE_BINARY_DIR}/games/botbyte"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/icon.ppm"
      "${CMAKE_BINARY_DIR}/games/botbyte"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/splash.ppm"
      "${CMAKE_BINARY_DIR}/games/botbyte"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:botbyte>"
      "${CMAKE_BINARY_DIR}/games/botbyte/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:botbyte>/SDL2.dll"
      "${CMAKE_BINARY_DIR}/games/botbyte/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:botbyte>/libgcc_s_seh-1.dll"
      "${CMAKE_BINARY_DIR}/games/botbyte/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:botbyte>/libstdc++-6.dll"
      "${CMAKE_BINARY_DIR}/games/botbyte/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:botbyte>/libwinpthread-1.dll"
      "${CMAKE_BINARY_DIR}/games/botbyte/bin"
    VERBATIM
  )
endif()

install(TARGETS botbyte DESTINATION opt/pixelpal/games/botbyte/bin)
install(FILES
  manifest.toml
  icon.ppm
  splash.ppm
  DESTINATION opt/pixelpal/games/botbyte
)
