find_package(SDL2 REQUIRED)

add_executable(pixelpal-checkers
  main.cpp
)

target_link_libraries(pixelpal-checkers
  PRIVATE
    pixelpal
    SDL2::SDL2
)

target_include_directories(pixelpal-checkers
  PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(pixelpal-checkers PRIVATE SDL_MAIN_HANDLED)

if(UNIX AND NOT APPLE)
  target_link_libraries(pixelpal-checkers PRIVATE m)
endif()

pixelpal_copy_windows_runtime_dlls(pixelpal-checkers)

if(WIN32)
  add_custom_command(
    TARGET pixelpal-checkers
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/games/pixelpal-checkers/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/manifest.toml"
      "${CMAKE_BINARY_DIR}/games/pixelpal-checkers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/icon.ppm"
      "${CMAKE_BINARY_DIR}/games/pixelpal-checkers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/splash.ppm"
      "${CMAKE_BINARY_DIR}/games/pixelpal-checkers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:pixelpal-checkers>"
      "${CMAKE_BINARY_DIR}/games/pixelpal-checkers/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:pixelpal-checkers>/SDL2.dll"
      "${CMAKE_BINARY_DIR}/games/pixelpal-checkers/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:pixelpal-checkers>/libgcc_s_seh-1.dll"
      "${CMAKE_BINARY_DIR}/games/pixelpal-checkers/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:pixelpal-checkers>/libstdc++-6.dll"
      "${CMAKE_BINARY_DIR}/games/pixelpal-checkers/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:pixelpal-checkers>/libwinpthread-1.dll"
      "${CMAKE_BINARY_DIR}/games/pixelpal-checkers/bin"
    VERBATIM
  )
endif()

install(TARGETS pixelpal-checkers DESTINATION opt/pixelpal/games/pixelpal-checkers/bin)
install(FILES
  manifest.toml
  icon.ppm
  splash.ppm
  DESTINATION opt/pixelpal/games/pixelpal-checkers
)
