find_package(SDL2 REQUIRED)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/pixelpal/build_info.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/pixelpal/build_info.hpp"
  @ONLY
)

add_executable(pixelpal-launcher
  src/catalog.cpp
  src/main.cpp
  src/menu_audio.cpp
  src/status_snapshot.cpp
)

target_include_directories(pixelpal-launcher
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_compile_features(pixelpal-launcher PRIVATE cxx_std_20)

target_compile_definitions(pixelpal-launcher PRIVATE SDL_MAIN_HANDLED)

target_link_libraries(pixelpal-launcher
  PRIVATE
    SDL2::SDL2
)

pixelpal_copy_windows_runtime_dlls(pixelpal-launcher)
pixelpal_stage_windows_theme(pixelpal-launcher)

install(TARGETS pixelpal-launcher DESTINATION usr/lib/pixelpal/launcher)
