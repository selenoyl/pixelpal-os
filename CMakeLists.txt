cmake_minimum_required(VERSION 3.20)

project(PixelPalOS LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PIXELPAL_BUILD_SAMPLE_GAMES "Build the sample games" ON)
option(PIXELPAL_BUILD_SHELVED_GAMES "Build shelved or paused sample games" OFF)

find_package(Git QUIET)

set(PIXELPAL_BUILD_NUMBER 1)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-list --count HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE PIXELPAL_GIT_COUNT_RESULT
    OUTPUT_VARIABLE PIXELPAL_GIT_COUNT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(PIXELPAL_GIT_COUNT_RESULT EQUAL 0 AND NOT PIXELPAL_GIT_COUNT STREQUAL "")
    set(PIXELPAL_BUILD_NUMBER "${PIXELPAL_GIT_COUNT}")
  else()
    include("${CMAKE_SOURCE_DIR}/cmake/PixelPalBuildInfo.cmake")
  endif()
else()
  include("${CMAKE_SOURCE_DIR}/cmake/PixelPalBuildInfo.cmake")
endif()

set(PIXELPAL_BUILD_NUMBER_PADDED "${PIXELPAL_BUILD_NUMBER}")
string(LENGTH "${PIXELPAL_BUILD_NUMBER_PADDED}" PIXELPAL_BUILD_NUMBER_LENGTH)
if(PIXELPAL_BUILD_NUMBER_LENGTH EQUAL 1)
  set(PIXELPAL_BUILD_NUMBER_PADDED "00${PIXELPAL_BUILD_NUMBER_PADDED}")
elseif(PIXELPAL_BUILD_NUMBER_LENGTH EQUAL 2)
  set(PIXELPAL_BUILD_NUMBER_PADDED "0${PIXELPAL_BUILD_NUMBER_PADDED}")
endif()
message(STATUS "PixelPal build label: BUILD ${PIXELPAL_BUILD_NUMBER_PADDED}")

function(pixelpal_copy_windows_runtime_dlls target_name)
  if(NOT WIN32)
    return()
  endif()

  get_filename_component(PIXELPAL_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
  foreach(runtime_dll
      SDL2.dll
      libgcc_s_seh-1.dll
      libstdc++-6.dll
      libwinpthread-1.dll)
    add_custom_command(
      TARGET ${target_name}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PIXELPAL_MINGW_BIN_DIR}/${runtime_dll}"
        "$<TARGET_FILE_DIR:${target_name}>"
      VERBATIM
    )
  endforeach()
endfunction()

function(pixelpal_stage_windows_theme target_name)
  if(NOT WIN32)
    return()
  endif()

  add_custom_command(
    TARGET ${target_name}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/themes/default"
      "${CMAKE_BINARY_DIR}/themes/default"
    VERBATIM
  )
endfunction()

add_subdirectory(sdk)
add_subdirectory(launcher)
add_subdirectory(tools/priory-editor)
add_subdirectory(tools/pixelpal-engine-cli)

if(PIXELPAL_BUILD_SAMPLE_GAMES)
  add_subdirectory(sample-games/botbite)
  add_subdirectory(sample-games/glitch-grid)
  add_subdirectory(sample-games/kilobytes)
  add_subdirectory(sample-games/molecules)
  add_subdirectory(sample-games/pixelpal-checkers)
  add_subdirectory(sample-games/pixelpal-chess)
  add_subdirectory(sample-games/pocket-port)
  add_subdirectory(sample-games/priory)
  add_subdirectory(sample-games/ribbon-raid)
  add_subdirectory(sample-games/riverfive)
  add_subdirectory(sample-games/settlers)
  add_subdirectory(sample-games/slinkbit)
  add_subdirectory(sample-games/stonefall)
  add_subdirectory(sample-games/vector-bastion)
  if(PIXELPAL_BUILD_SHELVED_GAMES)
    add_subdirectory(sample-games/brothers)
    add_subdirectory(sample-games/grand-prix)
  endif()
endif()
