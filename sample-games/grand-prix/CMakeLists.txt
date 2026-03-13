find_package(SDL2 REQUIRED)

add_executable(grandprix
  main.cpp
)

target_link_libraries(grandprix
  PRIVATE
    pixelpal
    SDL2::SDL2
)

target_include_directories(grandprix
  PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(grandprix PRIVATE SDL_MAIN_HANDLED)
target_compile_features(grandprix PRIVATE cxx_std_20)

if(UNIX AND NOT APPLE)
  target_link_libraries(grandprix PRIVATE m)
endif()

pixelpal_copy_windows_runtime_dlls(grandprix)

if(WIN32)
  add_custom_command(
    TARGET grandprix
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/games/grand-prix/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/manifest.toml"
      "${CMAKE_BINARY_DIR}/games/grand-prix"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/icon.ppm"
      "${CMAKE_BINARY_DIR}/games/grand-prix"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/splash.ppm"
      "${CMAKE_BINARY_DIR}/games/grand-prix"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:grandprix>"
      "${CMAKE_BINARY_DIR}/games/grand-prix/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:grandprix>/SDL2.dll"
      "${CMAKE_BINARY_DIR}/games/grand-prix/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:grandprix>/libgcc_s_seh-1.dll"
      "${CMAKE_BINARY_DIR}/games/grand-prix/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:grandprix>/libstdc++-6.dll"
      "${CMAKE_BINARY_DIR}/games/grand-prix/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:grandprix>/libwinpthread-1.dll"
      "${CMAKE_BINARY_DIR}/games/grand-prix/bin"
    VERBATIM
  )
endif()

install(TARGETS grandprix DESTINATION opt/pixelpal/games/grand-prix/bin)
install(FILES
  manifest.toml
  icon.ppm
  splash.ppm
  DESTINATION opt/pixelpal/games/grand-prix
)
