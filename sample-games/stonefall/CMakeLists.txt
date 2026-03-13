find_package(SDL2 REQUIRED)

add_executable(stonefall
  main.c
)

target_link_libraries(stonefall
  PRIVATE
    pixelpal
    SDL2::SDL2
)

target_include_directories(stonefall
  PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(stonefall PRIVATE SDL_MAIN_HANDLED)

if(UNIX AND NOT APPLE)
  target_link_libraries(stonefall PRIVATE m)
endif()

pixelpal_copy_windows_runtime_dlls(stonefall)

if(WIN32)
  add_custom_command(
    TARGET stonefall
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/games/stonefall/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/manifest.toml"
      "${CMAKE_BINARY_DIR}/games/stonefall"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/icon.ppm"
      "${CMAKE_BINARY_DIR}/games/stonefall"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/splash.ppm"
      "${CMAKE_BINARY_DIR}/games/stonefall"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:stonefall>"
      "${CMAKE_BINARY_DIR}/games/stonefall/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:stonefall>/SDL2.dll"
      "${CMAKE_BINARY_DIR}/games/stonefall/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:stonefall>/libgcc_s_seh-1.dll"
      "${CMAKE_BINARY_DIR}/games/stonefall/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:stonefall>/libstdc++-6.dll"
      "${CMAKE_BINARY_DIR}/games/stonefall/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:stonefall>/libwinpthread-1.dll"
      "${CMAKE_BINARY_DIR}/games/stonefall/bin"
    VERBATIM
  )
endif()

install(TARGETS stonefall DESTINATION opt/pixelpal/games/stonefall/bin)
install(FILES
  manifest.toml
  icon.ppm
  splash.ppm
  DESTINATION opt/pixelpal/games/stonefall
)
