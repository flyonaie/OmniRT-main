# Copyright (c) 2023, AgiBot Inc.
# All rights reserved.

include(FetchContent)

message(STATUS "get yaml-cpp ...")

# 设置本地源码路径
set(SRC_NAME "yaml-cpp")
message(STATUS "get ${SRC_NAME} print ...")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
set(yaml-cpp_LOCAL_SOURCE "${CMAKE_SOURCE_DIR}/_deps/${SRC_NAME}-src" CACHE PATH "Path to local ${SRC_NAME} source")

set(yaml-cpp_DOWNLOAD_URL
    "https://github.com/jbeder/yaml-cpp/archive/0.8.0.tar.gz"
    CACHE STRING "")

if(yaml-cpp_LOCAL_SOURCE)
  message(STATUS "using local yaml-cpp source: ${yaml-cpp_LOCAL_SOURCE}")
  FetchContent_Declare(
    yaml-cpp
    SOURCE_DIR ${yaml-cpp_LOCAL_SOURCE}
    OVERRIDE_FIND_PACKAGE)
else()
  message(STATUS "downloading yaml-cpp from ${yaml-cpp_DOWNLOAD_URL}")
  FetchContent_Declare(
    yaml-cpp
    URL ${yaml-cpp_DOWNLOAD_URL}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    OVERRIDE_FIND_PACKAGE)
endif()

FetchContent_GetProperties(yaml-cpp)
message(STATUS "yaml-cpp_POPULATED: ${yaml-cpp_POPULATED}")

if(NOT yaml-cpp_POPULATED)
  message(STATUS "populating yaml-cpp")
  set(BUILD_TESTING
      OFF
      CACHE BOOL "")
  set(YAML_CPP_BUILD_TESTS
      OFF
      CACHE BOOL "")
  set(YAML_CPP_BUILD_TOOLS
      OFF
      CACHE BOOL "")
  set(YAML_CPP_INSTALL
      ON
      CACHE BOOL "")
  set(YAML_CPP_FORMAT_SOURCE
      OFF
      CACHE BOOL "")
  set(YAML_CPP_BUILD_CONTRIB
      OFF
      CACHE BOOL "")
  FetchContent_MakeAvailable(yaml-cpp)
endif()

# import targets:
# yaml-cpp::yaml-cpp
