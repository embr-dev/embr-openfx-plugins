cmake_minimum_required(VERSION 3.18)
project(embr-openfx-plugins VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(EMBR_SAM2_USE_CUDA "Link ONNX Runtime CUDA provider if available" ON)
option(EMBR_FETCH_OPENFX "Fetch OpenFX SDK via FetchContent" ON)
option(EMBR_BUILD_SAM2 "Build EmbrSAM2 plugin" ON)
option(EMBR_BUILD_MATANYONE "Build EmbrMatAnyone2 plugin" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchOpenFX)
include(FindOnnxRuntime)

if(EMBR_BUILD_SAM2)
  add_subdirectory(plugins/sam2)
endif()
if(EMBR_BUILD_MATANYONE)
  add_subdirectory(plugins/matanyone2)
endif()

message(STATUS "OpenFX include: ${OPENFX_INCLUDE_DIR}")
message(STATUS "ONNX Runtime: ${ONNXRUNTIME_ROOT}")
