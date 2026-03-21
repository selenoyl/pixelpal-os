find_package(SDL2 REQUIRED)

add_executable(vector-bastion
  main.cpp
)

target_link_libraries(vector-bastion
  PRIVATE
    pixelpal
    SDL2::SDL2
)

target_include_directories(vector-bastion
  PRIVATE
    ${CMAKE_SOURCE_DIR}/sdk/include
)

target_compile_definitions(vector-bastion PRIVATE SDL_MAIN_HANDLED)
target_compile_features(vector-bastion PRIVATE cxx_std_20)

if(UNIX AND NOT APPLE)
  target_link_libraries(vector-bastion PRIVATE m)
endif()

pixelpal_copy_windows_runtime_dlls(vector-bastion)

if(WIN32)
  add_custom_command(
    TARGET vector-bastion
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/games/vector-bastion/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/manifest.toml"
      "${CMAKE_BINARY_DIR}/games/vector-bastion"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/icon.ppm"
      "${CMAKE_BINARY_DIR}/games/vector-bastion"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/splash.ppm"
      "${CMAKE_BINARY_DIR}/games/vector-bastion"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:vector-bastion>"
      "${CMAKE_BINARY_DIR}/games/vector-bastion/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:vector-bastion>/SDL2.dll"
      "${CMAKE_BINARY_DIR}/games/vector-bastion/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:vector-bastion>/libgcc_s_seh-1.dll"
      "${CMAKE_BINARY_DIR}/games/vector-bastion/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:vector-bastion>/libstdc++-6.dll"
      "${CMAKE_BINARY_DIR}/games/vector-bastion/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:vector-bastion>/libwinpthread-1.dll"
      "${CMAKE_BINARY_DIR}/games/vector-bastion/bin"
    VERBATIM
  )
endif()

install(TARGETS vector-bastion DESTINATION opt/pixelpal/games/vector-bastion/bin)
install(FILES
  manifest.toml
  icon.ppm
  splash.ppm
  DESTINATION opt/pixelpal/games/vector-bastion
)
