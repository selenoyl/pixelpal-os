find_package(SDL2 REQUIRED)

add_executable(kilobytes
  main.c
)

target_link_libraries(kilobytes
  PRIVATE
    pixelpal
    SDL2::SDL2
)

target_include_directories(kilobytes
  PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(kilobytes PRIVATE SDL_MAIN_HANDLED)

if(UNIX AND NOT APPLE)
  target_link_libraries(kilobytes PRIVATE m)
endif()

pixelpal_copy_windows_runtime_dlls(kilobytes)

if(WIN32)
  add_custom_command(
    TARGET kilobytes
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/games/kilobytes/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/manifest.toml"
      "${CMAKE_BINARY_DIR}/games/kilobytes"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/icon.ppm"
      "${CMAKE_BINARY_DIR}/games/kilobytes"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/splash.ppm"
      "${CMAKE_BINARY_DIR}/games/kilobytes"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:kilobytes>"
      "${CMAKE_BINARY_DIR}/games/kilobytes/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:kilobytes>/SDL2.dll"
      "${CMAKE_BINARY_DIR}/games/kilobytes/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:kilobytes>/libgcc_s_seh-1.dll"
      "${CMAKE_BINARY_DIR}/games/kilobytes/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:kilobytes>/libstdc++-6.dll"
      "${CMAKE_BINARY_DIR}/games/kilobytes/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:kilobytes>/libwinpthread-1.dll"
      "${CMAKE_BINARY_DIR}/games/kilobytes/bin"
    VERBATIM
  )
endif()

install(TARGETS kilobytes DESTINATION opt/pixelpal/games/kilobytes/bin)
install(FILES
  manifest.toml
  icon.ppm
  splash.ppm
  DESTINATION opt/pixelpal/games/kilobytes
)
