# Copyright (c) 2023, AgiBot Inc.
# All rights reserved.

include(FetchContent)

message(STATUS "get fmt ...")

# 设置本地源码路径（需要根据实际路径修改）
set(SRC_NAME "fmt")
message(STATUS "get ${SRC_NAME} print ...")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
set(fmt_LOCAL_SOURCE "${CMAKE_SOURCE_DIR}/_deps/${SRC_NAME}-src" CACHE PATH "Path to local ${SRC_NAME} source")


set(fmt_DOWNLOAD_URL
    "https://github.com/fmtlib/fmt/archive/10.2.1.tar.gz"
    CACHE STRING "")

if(fmt_LOCAL_SOURCE)
  FetchContent_Declare(
    fmt
    SOURCE_DIR ${fmt_LOCAL_SOURCE}
    OVERRIDE_FIND_PACKAGE)
else()
  FetchContent_Declare(
    fmt
    URL ${fmt_DOWNLOAD_URL}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    OVERRIDE_FIND_PACKAGE)
endif()

FetchContent_GetProperties(fmt)
if(NOT fmt_POPULATED)
  set(FMT_MASTER_PROJECT
      OFF
      CACHE BOOL "")
  set(FMT_INSTALL
      ${AIMRT_INSTALL}
      CACHE BOOL "")

  FetchContent_MakeAvailable(fmt)
endif()

# import targets：
# fmt::fmt
