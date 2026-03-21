find_package(SDL2 REQUIRED)

add_executable(ribbon-raid
  main.cpp
)

target_link_libraries(ribbon-raid
  PRIVATE
    pixelpal
    SDL2::SDL2
)

target_include_directories(ribbon-raid
  PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(ribbon-raid PRIVATE SDL_MAIN_HANDLED)

if(UNIX AND NOT APPLE)
  target_link_libraries(ribbon-raid PRIVATE m)
endif()

pixelpal_copy_windows_runtime_dlls(ribbon-raid)

if(WIN32)
  add_custom_command(
    TARGET ribbon-raid
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/games/ribbon-raid/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/manifest.toml"
      "${CMAKE_BINARY_DIR}/games/ribbon-raid"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/icon.ppm"
      "${CMAKE_BINARY_DIR}/games/ribbon-raid"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/splash.ppm"
      "${CMAKE_BINARY_DIR}/games/ribbon-raid"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:ribbon-raid>"
      "${CMAKE_BINARY_DIR}/games/ribbon-raid/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:ribbon-raid>/SDL2.dll"
      "${CMAKE_BINARY_DIR}/games/ribbon-raid/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:ribbon-raid>/libgcc_s_seh-1.dll"
      "${CMAKE_BINARY_DIR}/games/ribbon-raid/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:ribbon-raid>/libstdc++-6.dll"
      "${CMAKE_BINARY_DIR}/games/ribbon-raid/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:ribbon-raid>/libwinpthread-1.dll"
      "${CMAKE_BINARY_DIR}/games/ribbon-raid/bin"
    VERBATIM
  )
endif()

install(TARGETS ribbon-raid DESTINATION opt/pixelpal/games/ribbon-raid/bin)
install(FILES
  manifest.toml
  icon.ppm
  splash.ppm
  DESTINATION opt/pixelpal/games/ribbon-raid
)
