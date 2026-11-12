cmake_minimum_required(VERSION 3.20)
project(HandshapeSegmentationUpstream LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(
  HANDSHAPE_SEGMENTATION_ROOT
  "${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/HandshapeSegmentation"
  CACHE PATH
  "HandshapeSegmentation repository root")
if(NOT EXISTS "${HANDSHAPE_SEGMENTATION_ROOT}/src/matching/handskeleton.cpp")
  message(FATAL_ERROR "Set HANDSHAPE_SEGMENTATION_ROOT to the HandshapeSegmentation checkout")
endif()

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
FetchContent_Declare(
  eigen
  GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
  GIT_TAG 3.4.0
  GIT_SHALLOW TRUE)
FetchContent_Declare(
  ufbx
  GIT_REPOSITORY https://github.com/ufbx/ufbx.git
  GIT_TAG 1d097696e33b367bb3152b175aeb2d95193f9b6e
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR does_not_exist)
FetchContent_MakeAvailable(eigen ufbx)

set(NLOHMANN_JSON_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/nlohmann_json/include")
set(NLOHMANN_JSON_HEADER "${NLOHMANN_JSON_INCLUDE_DIR}/nlohmann/json.hpp")
if(NOT EXISTS "${NLOHMANN_JSON_HEADER}")
  file(MAKE_DIRECTORY "${NLOHMANN_JSON_INCLUDE_DIR}/nlohmann")
  file(DOWNLOAD
    "https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp"
    "${NLOHMANN_JSON_HEADER}"
    EXPECTED_HASH SHA256=9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6
    TLS_VERIFY ON)
endif()

set(BRIDGE_SOURCE Bridge/HandshapeSegmentationBridge.inl)
set_source_files_properties(${BRIDGE_SOURCE} PROPERTIES LANGUAGE CXX)

add_library(HandshapeSegmentationUpstream STATIC
  ${BRIDGE_SOURCE}
  ${HANDSHAPE_SEGMENTATION_ROOT}/src/lib/anim/anim.cpp
  ${HANDSHAPE_SEGMENTATION_ROOT}/src/lib/math/math.cpp
  ${HANDSHAPE_SEGMENTATION_ROOT}/src/matching/handshape.cpp
  ${HANDSHAPE_SEGMENTATION_ROOT}/src/matching/handskeleton.cpp
  ${HANDSHAPE_SEGMENTATION_ROOT}/src/segmentation/segmentation.cpp
  ${ufbx_SOURCE_DIR}/ufbx.c)

target_include_directories(HandshapeSegmentationUpstream
  PUBLIC Public
  PRIVATE
    ${HANDSHAPE_SEGMENTATION_ROOT}/src
    ${NLOHMANN_JSON_INCLUDE_DIR}
    ${ufbx_SOURCE_DIR})

target_link_libraries(HandshapeSegmentationUpstream
  PRIVATE
    Eigen3::Eigen)

set_target_properties(HandshapeSegmentationUpstream PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Lib/Win64"
  ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/Lib/Win64")
