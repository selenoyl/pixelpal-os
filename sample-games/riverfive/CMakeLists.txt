find_package(SDL2 REQUIRED)

add_executable(riverfive
  velvet_deck.cpp
)

target_link_libraries(riverfive
  PRIVATE
    pixelpal
    SDL2::SDL2
)

target_include_directories(riverfive
  PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(riverfive PRIVATE SDL_MAIN_HANDLED)

if(UNIX AND NOT APPLE)
  target_link_libraries(riverfive PRIVATE m)
endif()

pixelpal_copy_windows_runtime_dlls(riverfive)

if(WIN32)
  add_custom_command(
    TARGET riverfive
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/games/riverfive/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/manifest.toml"
      "${CMAKE_BINARY_DIR}/games/riverfive"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/icon.ppm"
      "${CMAKE_BINARY_DIR}/games/riverfive"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/splash.ppm"
      "${CMAKE_BINARY_DIR}/games/riverfive"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:riverfive>"
      "${CMAKE_BINARY_DIR}/games/riverfive/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:riverfive>/SDL2.dll"
      "${CMAKE_BINARY_DIR}/games/riverfive/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:riverfive>/libgcc_s_seh-1.dll"
      "${CMAKE_BINARY_DIR}/games/riverfive/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:riverfive>/libstdc++-6.dll"
      "${CMAKE_BINARY_DIR}/games/riverfive/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:riverfive>/libwinpthread-1.dll"
      "${CMAKE_BINARY_DIR}/games/riverfive/bin"
    VERBATIM
  )
endif()

install(TARGETS riverfive DESTINATION opt/pixelpal/games/riverfive/bin)
install(FILES
  manifest.toml
  icon.ppm
  splash.ppm
  DESTINATION opt/pixelpal/games/riverfive
)
