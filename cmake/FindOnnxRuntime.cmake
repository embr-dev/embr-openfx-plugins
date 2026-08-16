# Locate a pre-downloaded ONNX Runtime SDK.
# Expected layout (from official GitHub releases):
#   ${ONNXRUNTIME_ROOT}/include/onnxruntime_cxx_api.h
#   ${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so

if(NOT ONNXRUNTIME_ROOT)
  if(DEFINED ENV{ONNXRUNTIME_ROOT})
    set(ONNXRUNTIME_ROOT "$ENV{ONNXRUNTIME_ROOT}")
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/onnxruntime/include/onnxruntime_cxx_api.h")
    set(ONNXRUNTIME_ROOT "${CMAKE_SOURCE_DIR}/third_party/onnxruntime")
  endif()
endif()

if(NOT ONNXRUNTIME_ROOT)
  message(FATAL_ERROR
    "ONNX Runtime not found. Run scripts/download_onnxruntime_linux.sh "
    "or pass -DONNXRUNTIME_ROOT=/path/to/onnxruntime")
endif()

find_path(ONNXRUNTIME_INCLUDE_DIR
  NAMES onnxruntime_cxx_api.h
  PATHS "${ONNXRUNTIME_ROOT}/include"
  NO_DEFAULT_PATH
)

find_library(ONNXRUNTIME_LIBRARY
  NAMES onnxruntime
  PATHS "${ONNXRUNTIME_ROOT}/lib"
  NO_DEFAULT_PATH
)

if(NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIBRARY)
  message(FATAL_ERROR "Invalid ONNXRUNTIME_ROOT: ${ONNXRUNTIME_ROOT}")
endif()

set(ONNXRUNTIME_ROOT "${ONNXRUNTIME_ROOT}" CACHE PATH "ONNX Runtime root" FORCE)

add_library(onnxruntime_sdk INTERFACE)
target_include_directories(onnxruntime_sdk INTERFACE "${ONNXRUNTIME_INCLUDE_DIR}")
target_link_libraries(onnxruntime_sdk INTERFACE "${ONNXRUNTIME_LIBRARY}")
target_link_directories(onnxruntime_sdk INTERFACE "${ONNXRUNTIME_ROOT}/lib")
