find_package(SDL2 REQUIRED)

add_executable(priory
  main.cpp
)

target_link_libraries(priory
  PRIVATE
    pixelpal
    SDL2::SDL2
)

target_include_directories(priory
  PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(priory
  PRIVATE
    SDL_MAIN_HANDLED
    PRIORY_SOURCE_PROJECT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/editor-projects/saint-catherine-arrival.json"
)
target_compile_features(priory PRIVATE cxx_std_20)

if(UNIX AND NOT APPLE)
  target_link_libraries(priory PRIVATE m)
endif()

pixelpal_copy_windows_runtime_dlls(priory)

if(WIN32)
  add_custom_command(
    TARGET priory
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
      -Dsource_dir=${CMAKE_CURRENT_SOURCE_DIR}
      -Dexe_path=$<TARGET_FILE:priory>
      -Dexe_dir=$<TARGET_FILE_DIR:priory>
      -Ddest_root=${CMAKE_BINARY_DIR}/games/priory
      -P ${CMAKE_CURRENT_SOURCE_DIR}/stage-windows.cmake
    VERBATIM
  )
endif()

install(TARGETS priory DESTINATION opt/pixelpal/games/priory/bin)
install(FILES
  manifest.toml
  icon.ppm
  splash.ppm
  DESTINATION opt/pixelpal/games/priory
)
install(DIRECTORY
  editor-projects
  DESTINATION opt/pixelpal/games/priory
)
