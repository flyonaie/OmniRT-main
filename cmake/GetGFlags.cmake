# Copyright (c) 2023, AgiBot Inc.
# All rights reserved.

include(FetchContent)
include(CheckLocalSource)

message(STATUS "get gflags ...")

# 设置本地源码路径
set(SRC_NAME "gflags")
message(STATUS "get ${SRC_NAME} print ...")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")

# 检查本地源码目录是否存在且包含必要文件
check_local_source(
  SRC_NAME "${SRC_NAME}"
  SOURCE_DIR "_deps/${SRC_NAME}-src"
  REQUIRED_FILES "CMakeLists.txt" "src"
  RESULT_VAR "${SRC_NAME}_LOCAL_SOURCE"
)

if(gflags_LOCAL_SOURCE)
  set(gflags_LOCAL_SOURCE "${CMAKE_SOURCE_DIR}/${gflags_LOCAL_SOURCE}" CACHE PATH "Path to local ${SRC_NAME} source")
  message(STATUS "Found local ${SRC_NAME} source at: ${gflags_LOCAL_SOURCE}")
else()
  set(gflags_LOCAL_SOURCE "" CACHE PATH "Path to local ${SRC_NAME} source")
  message(STATUS "Local ${SRC_NAME} source not found or incomplete, will download from remote")
endif()

set(gflags_DOWNLOAD_URL
    "https://github.com/gflags/gflags/archive/v2.2.2.tar.gz"
    CACHE STRING "gflags download url" FORCE)
set(gflags_DOWNLOAD_SHA256
    "34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf"
    CACHE STRING "gflags download sha256" FORCE)

# 添加 CMP0063 策略设置
set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)

if(gflags_LOCAL_SOURCE)
  message(STATUS "using local gflags source: ${gflags_LOCAL_SOURCE}")
  FetchContent_Declare(
    gflags
    SOURCE_DIR ${gflags_LOCAL_SOURCE}
    OVERRIDE_FIND_PACKAGE)
else()
  FetchContent_Declare(
    gflags
    URL ${gflags_DOWNLOAD_URL}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    OVERRIDE_FIND_PACKAGE)
endif()

FetchContent_GetProperties(gflags)
if(NOT gflags_POPULATED)
  FetchContent_Populate(gflags)
  
  set(BUILD_TESTING
      OFF
      CACHE BOOL "")

  # 创建临时CMake策略文件
  file(WRITE "${gflags_SOURCE_DIR}/cmake_policies.cmake"
       "if(POLICY CMP0063)\n  cmake_policy(SET CMP0063 OLD)\nendif()\n")

  # 读取原始CMakeLists.txt
  file(READ ${gflags_SOURCE_DIR}/CMakeLists.txt TMP_VAR)
  
  # 在文件开头添加include语句
  file(WRITE ${gflags_SOURCE_DIR}/CMakeLists.txt 
       "include(${gflags_SOURCE_DIR}/cmake_policies.cmake)\n${TMP_VAR}")

  add_subdirectory(${gflags_SOURCE_DIR} ${gflags_BINARY_DIR})

endif()

# import targets:
# gflags::gflags
