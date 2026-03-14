add_executable(pixelpal-engine-cli
  main.cpp
)

target_link_libraries(pixelpal-engine-cli
  PRIVATE
    pixelpal_rpg_engine
)

target_compile_features(pixelpal-engine-cli PRIVATE cxx_std_20)

set_target_properties(pixelpal-engine-cli PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/pixelpal-engine-cli"
)

install(TARGETS pixelpal-engine-cli DESTINATION opt/pixelpal/tools)
