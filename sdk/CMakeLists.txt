find_package(SDL2 REQUIRED)

add_library(pixelpal STATIC
  src/pixelpal.c
)

target_include_directories(pixelpal
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pixelpal
  PUBLIC
    SDL2::SDL2
)

install(TARGETS pixelpal DESTINATION usr/lib/pixelpal/sdk)
install(FILES include/pixelpal/pixelpal.h DESTINATION usr/include/pixelpal)

