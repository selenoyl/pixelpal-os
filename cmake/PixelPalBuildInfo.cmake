set(_pixelpal_build_inputs
  "${CMAKE_SOURCE_DIR}/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/launcher/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/sdk/CMakeLists.txt"
)

foreach(_pixelpal_dir
    launcher
    sdk
    sample-games
    services
    themes)
  if(EXISTS "${CMAKE_SOURCE_DIR}/${_pixelpal_dir}")
    file(GLOB_RECURSE _pixelpal_dir_entries CONFIGURE_DEPENDS
      "${CMAKE_SOURCE_DIR}/${_pixelpal_dir}/*")
    list(APPEND _pixelpal_build_inputs ${_pixelpal_dir_entries})
  endif()
endforeach()

list(FILTER _pixelpal_build_inputs EXCLUDE REGEX "/build/")
list(FILTER _pixelpal_build_inputs EXCLUDE REGEX "\\.git/")
list(FILTER _pixelpal_build_inputs EXCLUDE REGEX "\\.vs/")
list(FILTER _pixelpal_build_inputs EXCLUDE REGEX "/out/")
list(FILTER _pixelpal_build_inputs EXCLUDE REGEX "\\.(user|tmp|log)$")
list(REMOVE_DUPLICATES _pixelpal_build_inputs)
list(SORT _pixelpal_build_inputs)

set(_pixelpal_fingerprint_material "")
foreach(_pixelpal_path IN LISTS _pixelpal_build_inputs)
  if(IS_DIRECTORY "${_pixelpal_path}")
    continue()
  endif()
  if(NOT EXISTS "${_pixelpal_path}")
    continue()
  endif()

  file(SHA256 "${_pixelpal_path}" _pixelpal_sha)
  file(RELATIVE_PATH _pixelpal_rel "${CMAKE_SOURCE_DIR}" "${_pixelpal_path}")
  string(APPEND _pixelpal_fingerprint_material "${_pixelpal_rel}:${_pixelpal_sha}\n")
endforeach()

string(SHA256 PIXELPAL_SOURCE_FINGERPRINT "${_pixelpal_fingerprint_material}")

set(_pixelpal_state_file "${CMAKE_BINARY_DIR}/pixelpal_build_state.cmake")
set(PIXELPAL_BUILD_NUMBER 1)

if(EXISTS "${_pixelpal_state_file}")
  include("${_pixelpal_state_file}")
  if(DEFINED PIXELPAL_LAST_SOURCE_FINGERPRINT AND
     PIXELPAL_LAST_SOURCE_FINGERPRINT STREQUAL PIXELPAL_SOURCE_FINGERPRINT AND
     DEFINED PIXELPAL_LAST_BUILD_NUMBER)
    set(PIXELPAL_BUILD_NUMBER "${PIXELPAL_LAST_BUILD_NUMBER}")
  elseif(DEFINED PIXELPAL_LAST_BUILD_NUMBER)
    math(EXPR PIXELPAL_BUILD_NUMBER "${PIXELPAL_LAST_BUILD_NUMBER} + 1")
  endif()
endif()

file(WRITE "${_pixelpal_state_file}"
  "set(PIXELPAL_LAST_SOURCE_FINGERPRINT \"${PIXELPAL_SOURCE_FINGERPRINT}\")\n"
  "set(PIXELPAL_LAST_BUILD_NUMBER \"${PIXELPAL_BUILD_NUMBER}\")\n")
