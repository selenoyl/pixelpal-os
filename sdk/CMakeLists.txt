find_package(SDL2 REQUIRED)

add_library(pixelpal STATIC
  src/pixelpal.c
)

add_library(pixelpal_rpg_engine STATIC
  src/rpg_engine.cpp
)

target_include_directories(pixelpal
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(pixelpal_rpg_engine
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pixelpal
  PUBLIC
    SDL2::SDL2
)

target_compile_features(pixelpal_rpg_engine PUBLIC cxx_std_20)

install(TARGETS pixelpal DESTINATION usr/lib/pixelpal/sdk)
install(TARGETS pixelpal_rpg_engine DESTINATION usr/lib/pixelpal/sdk)
install(FILES
  include/pixelpal/pixelpal.h
  include/pixelpal/rpg_engine.hpp
  DESTINATION usr/include/pixelpal
)
