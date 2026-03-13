find_package(SDL2 REQUIRED)

add_executable(slinkbit
  main.c
)

target_link_libraries(slinkbit
  PRIVATE
    pixelpal
    SDL2::SDL2
)

target_include_directories(slinkbit
  PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(slinkbit PRIVATE SDL_MAIN_HANDLED)

if(UNIX AND NOT APPLE)
  target_link_libraries(slinkbit PRIVATE m)
endif()

pixelpal_copy_windows_runtime_dlls(slinkbit)

if(WIN32)
  add_custom_command(
    TARGET slinkbit
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/games/slinkbit/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/manifest.toml"
      "${CMAKE_BINARY_DIR}/games/slinkbit"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/icon.ppm"
      "${CMAKE_BINARY_DIR}/games/slinkbit"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/splash.ppm"
      "${CMAKE_BINARY_DIR}/games/slinkbit"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:slinkbit>"
      "${CMAKE_BINARY_DIR}/games/slinkbit/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:slinkbit>/SDL2.dll"
      "${CMAKE_BINARY_DIR}/games/slinkbit/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:slinkbit>/libgcc_s_seh-1.dll"
      "${CMAKE_BINARY_DIR}/games/slinkbit/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:slinkbit>/libstdc++-6.dll"
      "${CMAKE_BINARY_DIR}/games/slinkbit/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:slinkbit>/libwinpthread-1.dll"
      "${CMAKE_BINARY_DIR}/games/slinkbit/bin"
    VERBATIM
  )
endif()

install(TARGETS slinkbit DESTINATION opt/pixelpal/games/slinkbit/bin)
install(FILES
  manifest.toml
  icon.ppm
  splash.ppm
  DESTINATION opt/pixelpal/games/slinkbit
)
