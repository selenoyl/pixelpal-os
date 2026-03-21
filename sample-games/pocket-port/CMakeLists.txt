find_package(SDL2 REQUIRED)

add_executable(pocket-port
  main.cpp
)

target_link_libraries(pocket-port
  PRIVATE
    pixelpal
    SDL2::SDL2
)

target_include_directories(pocket-port
  PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(pocket-port PRIVATE SDL_MAIN_HANDLED)

if(UNIX AND NOT APPLE)
  target_link_libraries(pocket-port PRIVATE m)
endif()

pixelpal_copy_windows_runtime_dlls(pocket-port)

if(WIN32)
  add_custom_command(
    TARGET pocket-port
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/games/pocket-port/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/manifest.toml"
      "${CMAKE_BINARY_DIR}/games/pocket-port"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/icon.ppm"
      "${CMAKE_BINARY_DIR}/games/pocket-port"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/splash.ppm"
      "${CMAKE_BINARY_DIR}/games/pocket-port"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:pocket-port>"
      "${CMAKE_BINARY_DIR}/games/pocket-port/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:pocket-port>/SDL2.dll"
      "${CMAKE_BINARY_DIR}/games/pocket-port/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:pocket-port>/libgcc_s_seh-1.dll"
      "${CMAKE_BINARY_DIR}/games/pocket-port/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:pocket-port>/libstdc++-6.dll"
      "${CMAKE_BINARY_DIR}/games/pocket-port/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:pocket-port>/libwinpthread-1.dll"
      "${CMAKE_BINARY_DIR}/games/pocket-port/bin"
    VERBATIM
  )
endif()

install(TARGETS pocket-port DESTINATION opt/pixelpal/games/pocket-port/bin)
install(FILES
  manifest.toml
  icon.ppm
  splash.ppm
  DESTINATION opt/pixelpal/games/pocket-port
)
