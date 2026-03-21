find_package(SDL2 REQUIRED)

add_executable(settlers
  main.cpp
)

target_link_libraries(settlers
  PRIVATE
    pixelpal
    SDL2::SDL2
)

target_include_directories(settlers
  PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(settlers PRIVATE SDL_MAIN_HANDLED)
target_compile_features(settlers PRIVATE cxx_std_20)

if(UNIX AND NOT APPLE)
  target_link_libraries(settlers PRIVATE m)
endif()

pixelpal_copy_windows_runtime_dlls(settlers)

if(WIN32)
  add_custom_command(
    TARGET settlers
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/games/settlers/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/manifest.toml"
      "${CMAKE_BINARY_DIR}/games/settlers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/icon.ppm"
      "${CMAKE_BINARY_DIR}/games/settlers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/splash.ppm"
      "${CMAKE_BINARY_DIR}/games/settlers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:settlers>"
      "${CMAKE_BINARY_DIR}/games/settlers/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:settlers>/SDL2.dll"
      "${CMAKE_BINARY_DIR}/games/settlers/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:settlers>/libgcc_s_seh-1.dll"
      "${CMAKE_BINARY_DIR}/games/settlers/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:settlers>/libstdc++-6.dll"
      "${CMAKE_BINARY_DIR}/games/settlers/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:settlers>/libwinpthread-1.dll"
      "${CMAKE_BINARY_DIR}/games/settlers/bin"
    VERBATIM
  )
endif()

install(TARGETS settlers DESTINATION opt/pixelpal/games/settlers/bin)
install(FILES
  manifest.toml
  icon.ppm
  splash.ppm
  DESTINATION opt/pixelpal/games/settlers
)
