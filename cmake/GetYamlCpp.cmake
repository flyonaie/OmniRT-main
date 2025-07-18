# Copyright (c) 2023, AgiBot Inc.
# All rights reserved.

include(FetchContent)
include(CheckLocalSource)

# 设置本地源码路径
set(SRC_NAME "yaml-cpp")
message(STATUS "get ${SRC_NAME} ...")

#-----------------step1:检查指定目录下的库是否存在-----------------------#
# 清除 find_package 的缓存
unset(${SRC_NAME}_DIR CACHE)
unset(${SRC_NAME}_FOUND CACHE)

# 下面这句话和find_package中PATHS用一个即可
# set(${SRC_NAME}_DIR "${CMAKE_PREFIX_PATH}/${SRC_NAME}" CACHE PATH "Path to ${SRC_NAME} config file" FORCE)
message(STATUS "555555555555CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")
    
# 只在指定目录中查找库
find_package(${SRC_NAME} QUIET
        NO_DEFAULT_PATH
        PATHS "${CMAKE_PREFIX_PATH}/${SRC_NAME}")

if(${SRC_NAME}_FOUND)
    message(STATUS "${SRC_NAME}found via find_package at: ${${SRC_NAME}_DIR}")
    return()
else()
    message(STATUS "${SRC_NAME} not found via find_package, attempting to build from source")
endif()

#-----------------step2:查本地源码目录是否存-----------------------#
message(STATUS "check source ${FETCHCONTENT_BASE_DIR}/${SRC_NAME}-src")
check_local_source(
    SRC_NAME "${SRC_NAME}"
    SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/${SRC_NAME}-src"
    REQUIRED_FILES "CMakeLists.txt" "src"
    RESULT_VAR "${SRC_NAME}_LOCAL_SOURCE"
)

set(yaml-cpp_DOWNLOAD_URL
    "https://github.com/jbeder/yaml-cpp/archive/0.8.0.tar.gz"
    CACHE STRING "")

message(STATUS "yaml-cpp_LOCAL_SOURCE: ${yaml-cpp_LOCAL_SOURCE}")
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
message(STATUS "11yaml-cpp_POPULATED: ${yaml-cpp_POPULATED}")

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

    # 设置 fmt 的编译输出目录为 out
    # set(CMAKE_BINARY_DIR ${THIRD_PARTY_OUTPUT_DIR}/build)
    # set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${THIRD_PARTY_OUTPUT_DIR}/build/lib)
    # set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${THIRD_PARTY_OUTPUT_DIR}/build/lib)

    # # 设置 fmt 的安装目录为 out
    # set(CMAKE_INSTALL_PREFIX ${THIRD_PARTY_OUTPUT_DIR}/install)
    message(STATUS "555555555555CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")
    
  FetchContent_MakeAvailable(yaml-cpp)
  message(STATUS "22yaml-cpp_POPULATED: ${yaml-cpp_POPULATED}")
endif()

# import targets:
# yaml-cpp::yaml-cpp
