cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED source_dir OR NOT DEFINED exe_path OR NOT DEFINED exe_dir OR NOT DEFINED dest_root)
  message(FATAL_ERROR "stage-windows.cmake requires source_dir, exe_path, exe_dir, and dest_root.")
endif()

file(MAKE_DIRECTORY "${dest_root}/bin")

foreach(asset manifest.toml icon.ppm splash.ppm)
  file(COPY "${source_dir}/${asset}" DESTINATION "${dest_root}")
endforeach()

if(EXISTS "${source_dir}/editor-projects")
  file(COPY "${source_dir}/editor-projects" DESTINATION "${dest_root}")
endif()

file(COPY "${exe_path}" DESTINATION "${dest_root}/bin")

foreach(runtime_dll
    SDL2.dll
    libgcc_s_seh-1.dll
    libstdc++-6.dll
    libwinpthread-1.dll)
  if(EXISTS "${exe_dir}/${runtime_dll}")
    file(COPY "${exe_dir}/${runtime_dll}" DESTINATION "${dest_root}/bin")
  endif()
endforeach()
