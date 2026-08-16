set(SAM2_PLUGIN_SOURCES
  src/Sam2Engine.cpp
  src/Sam2OfxPlugin.cpp
)

add_library(EmbrSAM2 MODULE ${SAM2_PLUGIN_SOURCES} ${OPENFX_SUPPORT_SOURCES})

target_include_directories(EmbrSAM2
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OPENFX_INCLUDE_DIR}
    ${OPENFX_SUPPORT_INCLUDE_DIR}
)

target_link_libraries(EmbrSAM2 PRIVATE onnxruntime_sdk)

# OpenFX plugins are named *.ofx
set_target_properties(EmbrSAM2 PROPERTIES
  PREFIX ""
  SUFFIX ".ofx"
  OUTPUT_NAME "EmbrSAM2"
)

if(UNIX AND NOT APPLE)
  target_link_libraries(EmbrSAM2 PRIVATE dl pthread)
endif()

# Optional CUDA compile definition when GPU ORT is detected.
if(EMBR_SAM2_USE_CUDA AND EXISTS "${ONNXRUNTIME_ROOT}/lib/libonnxruntime_providers_cuda.so")
  target_compile_definitions(EmbrSAM2 PRIVATE EMBR_SAM2_HAS_CUDA=1)
  message(STATUS "SAM2: enabling CUDA execution provider")
endif()

# Package as OFX bundle for Linux hosts (Flame/Resolve/Natron).
set(OFX_BUNDLE_DIR "${CMAKE_BINARY_DIR}/ofx/EmbrSAM2.ofx.bundle/Contents")
add_custom_command(TARGET EmbrSAM2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${OFX_BUNDLE_DIR}/Linux-x86-64"
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:EmbrSAM2> "${OFX_BUNDLE_DIR}/Linux-x86-64/EmbrSAM2.ofx"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
          "${OFX_BUNDLE_DIR}/Info.plist"
  COMMENT "Packaging EmbrSAM2.ofx.bundle"
)

# Host-free smoke test for the inference engine (demo mode).
add_executable(sam2_engine_smoke src/sam2_engine_smoke.cpp src/Sam2Engine.cpp)
target_include_directories(sam2_engine_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(sam2_engine_smoke PRIVATE onnxruntime_sdk)
if(UNIX AND NOT APPLE)
  target_link_libraries(sam2_engine_smoke PRIVATE dl pthread)
endif()
