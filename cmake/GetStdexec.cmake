# Copyright (c) 2023, AgiBot Inc.
# All rights reserved.

include(FetchContent)
include(CheckLocalSource)

# 设置本地源码路径（需要根据实际路径修改）
set(SRC_NAME "stdexec")
message(STATUS "get ${SRC_NAME} ...")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
set(fmt_LOCAL_SOURCE "${CMAKE_SOURCE_DIR}/_deps/${SRC_NAME}-src" CACHE PATH "Path to local ${SRC_NAME} source")

# 检查本地源码目录是否存在且包含必要文件
check_local_source(
  SRC_NAME "${SRC_NAME}"
  SOURCE_DIR "_deps/${SRC_NAME}-src"
  REQUIRED_FILES "CMakeLists.txt"
  RESULT_VAR "${SRC_NAME}_LOCAL_SOURCE"
)

message(STATUS "stdexec_LOCAL_SOURCE: ${stdexec_LOCAL_SOURCE}")

set(stdexec_DOWNLOAD_URL
    "https://github.com/NVIDIA/stdexec/archive/nvhpc-23.09.rc4.tar.gz"
    CACHE STRING "")

if(stdexec_LOCAL_SOURCE)
  FetchContent_Declare(
    stdexec
    SOURCE_DIR ${stdexec_LOCAL_SOURCE}
    OVERRIDE_FIND_PACKAGE)
else()
  FetchContent_Declare(
    stdexec
    URL ${stdexec_DOWNLOAD_URL}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    OVERRIDE_FIND_PACKAGE)
endif()

FetchContent_GetProperties(stdexec)
if(NOT stdexec_POPULATED)
  set(STDEXEC_ENABLE_IO_URING_TESTS
      OFF
      CACHE BOOL "")

  set(STDEXEC_BUILD_EXAMPLES
      OFF
      CACHE BOOL "")

  set(STDEXEC_BUILD_TESTS
      OFF
      CACHE BOOL "")

  FetchContent_MakeAvailable(stdexec)
endif()

# import targets:
# STDEXEC::stdexec
# STDEXEC::nvexec
# STDEXEC::tbbexec
