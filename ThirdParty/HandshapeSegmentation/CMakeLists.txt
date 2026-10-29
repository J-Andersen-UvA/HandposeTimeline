cmake_minimum_required(VERSION 3.20)
project(HandshapeSegmentationUpstream LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(HANDSHAPE_SEGMENTATION_ROOT "" CACHE PATH "HandshapeSegmentation repository root")
if(NOT EXISTS "${HANDSHAPE_SEGMENTATION_ROOT}/src/matching/poseshape.cpp")
  message(FATAL_ERROR "Set HANDSHAPE_SEGMENTATION_ROOT to the HandshapeSegmentation checkout")
endif()

find_package(Eigen3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

include(FetchContent)
FetchContent_Declare(
  ufbx
  GIT_REPOSITORY https://github.com/ufbx/ufbx.git
  GIT_TAG master
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR does_not_exist)
FetchContent_MakeAvailable(ufbx)

add_library(HandshapeSegmentationUpstream STATIC
  src/HandshapeSegmentationBridge.cpp
  ${HANDSHAPE_SEGMENTATION_ROOT}/src/anim/anim.cpp
  ${HANDSHAPE_SEGMENTATION_ROOT}/src/math/math.cpp
  ${HANDSHAPE_SEGMENTATION_ROOT}/src/matching/poseshape.cpp
  ${HANDSHAPE_SEGMENTATION_ROOT}/src/segmentation.cpp
  ${ufbx_SOURCE_DIR}/ufbx.c)

target_include_directories(HandshapeSegmentationUpstream
  PUBLIC include
  PRIVATE
    ${HANDSHAPE_SEGMENTATION_ROOT}/src
    ${ufbx_SOURCE_DIR})

target_link_libraries(HandshapeSegmentationUpstream
  PRIVATE
    Eigen3::Eigen
    nlohmann_json::nlohmann_json)

set_target_properties(HandshapeSegmentationUpstream PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib/Win64"
  ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Win64")
