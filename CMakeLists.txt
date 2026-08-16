cmake_minimum_required(VERSION 3.18)
project(embr-openfx-plugins VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(EMBR_SAM2_USE_CUDA "Link ONNX Runtime CUDA provider if available" ON)
option(EMBR_FETCH_OPENFX "Fetch OpenFX SDK via FetchContent" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchOpenFX)
include(FindOnnxRuntime)

add_subdirectory(plugins/sam2)

message(STATUS "OpenFX include: ${OPENFX_INCLUDE_DIR}")
message(STATUS "ONNX Runtime: ${ONNXRUNTIME_ROOT}")
