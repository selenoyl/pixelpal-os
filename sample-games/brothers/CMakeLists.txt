find_package(SDL2 REQUIRED)

add_executable(brothers
  main.c
)

target_link_libraries(brothers
  PRIVATE
    pixelpal
    SDL2::SDL2
)

target_include_directories(brothers
  PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(brothers PRIVATE SDL_MAIN_HANDLED)

if(UNIX AND NOT APPLE)
  target_link_libraries(brothers PRIVATE m)
endif()

pixelpal_copy_windows_runtime_dlls(brothers)

if(WIN32)
  add_custom_command(
    TARGET brothers
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/games/brothers/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/manifest.toml"
      "${CMAKE_BINARY_DIR}/games/brothers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/icon.ppm"
      "${CMAKE_BINARY_DIR}/games/brothers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/splash.ppm"
      "${CMAKE_BINARY_DIR}/games/brothers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:brothers>"
      "${CMAKE_BINARY_DIR}/games/brothers/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:brothers>/SDL2.dll"
      "${CMAKE_BINARY_DIR}/games/brothers/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:brothers>/libgcc_s_seh-1.dll"
      "${CMAKE_BINARY_DIR}/games/brothers/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:brothers>/libstdc++-6.dll"
      "${CMAKE_BINARY_DIR}/games/brothers/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:brothers>/libwinpthread-1.dll"
      "${CMAKE_BINARY_DIR}/games/brothers/bin"
    VERBATIM
  )
endif()

install(TARGETS brothers DESTINATION opt/pixelpal/games/brothers/bin)
install(FILES
  manifest.toml
  icon.ppm
  splash.ppm
  DESTINATION opt/pixelpal/games/brothers
)
