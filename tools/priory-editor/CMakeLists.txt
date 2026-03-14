find_package(SDL2 REQUIRED)

add_executable(priory-editor
  main.cpp
)

target_link_libraries(priory-editor
  PRIVATE
    SDL2::SDL2
)

target_compile_definitions(priory-editor PRIVATE SDL_MAIN_HANDLED)
target_compile_features(priory-editor PRIVATE cxx_std_20)

if(UNIX AND NOT APPLE)
  target_link_libraries(priory-editor PRIVATE m)
endif()

set_target_properties(priory-editor PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/priory-editor"
)

pixelpal_copy_windows_runtime_dlls(priory-editor)

install(TARGETS priory-editor DESTINATION opt/pixelpal/tools)
