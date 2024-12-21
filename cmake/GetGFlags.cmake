# Copyright (c) 2023, AgiBot Inc.
# All rights reserved.

include(FetchContent)
include(CheckLocalSource)

# 在最开始就设置 CMP0063 策略
cmake_policy(SET CMP0063 NEW)

# 设置本地源码路径（需要根据实际路径修改）
set(SRC_NAME "gflags")
message(STATUS "get ${SRC_NAME} ...")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")

# 检查本地源码目录是否存在且包含必要文件
check_local_source(
  SRC_NAME "${SRC_NAME}"
  SOURCE_DIR "_deps/${SRC_NAME}-src"
  REQUIRED_FILES "CMakeLists.txt" "src"
  RESULT_VAR "${SRC_NAME}_LOCAL_SOURCE"
)

set(gflags_DOWNLOAD_URL
    "https://github.com/gflags/gflags/archive/v2.2.2.tar.gz"
    CACHE STRING "")

if(gflags_LOCAL_SOURCE)
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

# Wrap it in a function to restrict the scope of the variables
function(get_gflags)
  FetchContent_GetProperties(gflags)
  if(NOT gflags_POPULATED)
    FetchContent_Populate(gflags)
    
    # 设置 gflags 特定的构建选项
    set(BUILD_TESTING OFF CACHE BOOL "Disable gflags testing" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE BOOL "Build static libraries" FORCE)
    set(BUILD_gflags_LIB ON CACHE BOOL "Build gflags library" FORCE)
    set(BUILD_gflags_nothreads_LIB OFF CACHE BOOL "Disable no-threads library" FORCE)
    set(GFLAGS_USE_TARGET_NAMESPACE ON CACHE BOOL "Use target namespace" FORCE)
    
    # 设置可见性相关选项
    set(CMAKE_CXX_VISIBILITY_PRESET hidden CACHE STRING "Symbol visibility preset" FORCE)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN YES CACHE BOOL "Hide inlines" FORCE)
    set(CMAKE_POLICY_DEFAULT_CMP0063 NEW CACHE STRING "Set CMP0063 policy" FORCE)
    
    # 修改 gflags 的 CMakeLists.txt 文件
    file(READ ${gflags_SOURCE_DIR}/CMakeLists.txt TMP_VAR)
    string(REPLACE "  set (PKGCONFIG_INSTALL_DIR " "# set (PKGCONFIG_INSTALL_DIR " TMP_VAR "${TMP_VAR}")
    
    # 添加必要的策略设置和可见性控制
    set(NEW_CMAKE_CONTENT 
"cmake_policy(SET CMP0063 NEW)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
${TMP_VAR}")
    
    file(WRITE ${gflags_SOURCE_DIR}/CMakeLists.txt "${NEW_CMAKE_CONTENT}")

    # 添加子目录
    add_subdirectory(${gflags_SOURCE_DIR} ${gflags_BINARY_DIR})
  endif()
endfunction()

get_gflags()

# import targets:
# gflags::gflags