find_package(SDL2 REQUIRED)

add_executable(molecules
  main.cpp
)

target_link_libraries(molecules
  PRIVATE
    pixelpal
    SDL2::SDL2
)

target_include_directories(molecules
  PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(molecules PRIVATE SDL_MAIN_HANDLED)

if(UNIX AND NOT APPLE)
  target_link_libraries(molecules PRIVATE m)
endif()

pixelpal_copy_windows_runtime_dlls(molecules)

if(WIN32)
  add_custom_command(
    TARGET molecules
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/games/molecules/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/manifest.toml"
      "${CMAKE_BINARY_DIR}/games/molecules"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/icon.ppm"
      "${CMAKE_BINARY_DIR}/games/molecules"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/splash.ppm"
      "${CMAKE_BINARY_DIR}/games/molecules"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:molecules>"
      "${CMAKE_BINARY_DIR}/games/molecules/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:molecules>/SDL2.dll"
      "${CMAKE_BINARY_DIR}/games/molecules/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:molecules>/libgcc_s_seh-1.dll"
      "${CMAKE_BINARY_DIR}/games/molecules/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:molecules>/libstdc++-6.dll"
      "${CMAKE_BINARY_DIR}/games/molecules/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:molecules>/libwinpthread-1.dll"
      "${CMAKE_BINARY_DIR}/games/molecules/bin"
    VERBATIM
  )
endif()

install(TARGETS molecules DESTINATION opt/pixelpal/games/molecules/bin)
install(FILES
  manifest.toml
  icon.ppm
  splash.ppm
  DESTINATION opt/pixelpal/games/molecules
)
