set(MAT_SOURCES
  src/MatAnyoneEngine.cpp
  src/MatAnyoneNeuralBackend.cpp
  src/MatAnyoneOfxPlugin.cpp
)

add_library(EmbrMatAnyone2 MODULE ${MAT_SOURCES} ${OPENFX_SUPPORT_SOURCES})

target_include_directories(EmbrMatAnyone2
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OPENFX_INCLUDE_DIR}
    ${OPENFX_SUPPORT_INCLUDE_DIR}
)

set_target_properties(EmbrMatAnyone2 PROPERTIES
  PREFIX ""
  SUFFIX ".ofx"
  OUTPUT_NAME "EmbrMatAnyone2"
  BUILD_RPATH "\$ORIGIN"
  INSTALL_RPATH "\$ORIGIN"
)

if(UNIX AND NOT APPLE)
  target_link_libraries(EmbrMatAnyone2 PRIVATE dl pthread)
endif()

set(OFX_BUNDLE_DIR "${CMAKE_BINARY_DIR}/ofx/EmbrMatAnyone2.ofx.bundle/Contents")
set(OFX_BUNDLE_BIN "${OFX_BUNDLE_DIR}/Linux-x86-64")

add_custom_command(TARGET EmbrMatAnyone2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${OFX_BUNDLE_BIN}"
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:EmbrMatAnyone2> "${OFX_BUNDLE_BIN}/EmbrMatAnyone2.ofx"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
          "${OFX_BUNDLE_DIR}/Info.plist"
  COMMENT "Packaging EmbrMatAnyone2.ofx.bundle"
)

add_executable(matanyone2_engine_smoke
  src/matanyone2_engine_smoke.cpp
  src/MatAnyoneEngine.cpp
  src/MatAnyoneNeuralBackend.cpp
)
target_include_directories(matanyone2_engine_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(UNIX AND NOT APPLE)
  target_link_libraries(matanyone2_engine_smoke PRIVATE pthread)
endif()

add_executable(matanyone2_neural_smoke
  src/matanyone2_neural_smoke.cpp
  src/MatAnyoneEngine.cpp
  src/MatAnyoneNeuralBackend.cpp
)
target_include_directories(matanyone2_neural_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(UNIX AND NOT APPLE)
  target_link_libraries(matanyone2_neural_smoke PRIVATE pthread)
endif()
