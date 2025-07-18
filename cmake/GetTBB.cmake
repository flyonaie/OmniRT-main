# Copyright (c) 2023, AgiBot Inc.
# All rights reserved.

include(FetchContent)
include(CheckLocalSource)

# 设置本地源码路径
set(SRC_NAME "tbb")
message(STATUS "get ${SRC_NAME} print ...")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")

# 检查本地源码目录是否存在且包含必要文件
check_local_source(
  SRC_NAME "${SRC_NAME}"
  SOURCE_DIR "_deps/${SRC_NAME}-src"
  REQUIRED_FILES "CMakeLists.txt" "src"
  RESULT_VAR "${SRC_NAME}_LOCAL_SOURCE"
)

set(tbb_DOWNLOAD_URL
    "https://github.com/oneapi-src/oneTBB/archive/v2021.13.0.tar.gz"
    CACHE STRING "")

if(tbb_LOCAL_SOURCE)
  FetchContent_Declare(
    tbb
    SOURCE_DIR ${tbb_LOCAL_SOURCE}
    OVERRIDE_FIND_PACKAGE)
else()
  FetchContent_Declare(
    tbb
    URL ${tbb_DOWNLOAD_URL}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    OVERRIDE_FIND_PACKAGE)
endif()

# Wrap it in a function to restrict the scope of the variables
function(get_tbb)
  FetchContent_GetProperties(tbb)
  if(NOT tbb_POPULATED)
    set(TBB_TEST OFF)

    set(TBB_DIR "")

    set(TBB_INSTALL ON)

    set(TBB_STRICT OFF)

    FetchContent_MakeAvailable(tbb)
  endif()
endfunction()

get_tbb()

# import targets:
# TBB::tbb
