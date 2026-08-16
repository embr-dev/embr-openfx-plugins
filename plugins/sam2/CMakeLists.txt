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

# Resolve libonnxruntime.so from the same directory as EmbrSAM2.ofx ($ORIGIN).
set_target_properties(EmbrSAM2 PROPERTIES
  PREFIX ""
  SUFFIX ".ofx"
  OUTPUT_NAME "EmbrSAM2"
  BUILD_RPATH "\$ORIGIN"
  INSTALL_RPATH "\$ORIGIN"
)

if(UNIX AND NOT APPLE)
  target_link_libraries(EmbrSAM2 PRIVATE dl pthread)
endif()

if(EMBR_SAM2_USE_CUDA AND EXISTS "${ONNXRUNTIME_ROOT}/lib/libonnxruntime_providers_cuda.so")
  target_compile_definitions(EmbrSAM2 PRIVATE EMBR_SAM2_HAS_CUDA=1)
  message(STATUS "SAM2: enabling CUDA execution provider")
endif()

set(OFX_BUNDLE_DIR "${CMAKE_BINARY_DIR}/ofx/EmbrSAM2.ofx.bundle/Contents")
set(OFX_BUNDLE_BIN "${OFX_BUNDLE_DIR}/Linux-x86-64")

file(GLOB ORT_RUNTIME_LIBS
  "${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so"
  "${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so.*"
  "${ONNXRUNTIME_ROOT}/lib/libonnxruntime_providers_*.so"
)

set(_ort_copy_cmds)
foreach(_lib IN LISTS ORT_RUNTIME_LIBS)
  list(APPEND _ort_copy_cmds
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_lib}" "${OFX_BUNDLE_BIN}/")
endforeach()

add_custom_command(TARGET EmbrSAM2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${OFX_BUNDLE_BIN}"
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:EmbrSAM2> "${OFX_BUNDLE_BIN}/EmbrSAM2.ofx"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
          "${OFX_BUNDLE_DIR}/Info.plist"
  ${_ort_copy_cmds}
  COMMENT "Packaging EmbrSAM2.ofx.bundle with ONNX Runtime libs"
)

add_executable(sam2_engine_smoke src/sam2_engine_smoke.cpp src/Sam2Engine.cpp)
target_include_directories(sam2_engine_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(sam2_engine_smoke PRIVATE onnxruntime_sdk)
if(UNIX AND NOT APPLE)
  target_link_libraries(sam2_engine_smoke PRIVATE dl pthread)
endif()
