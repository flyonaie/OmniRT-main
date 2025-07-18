# Copyright (c) 2023, AgiBot Inc.
# All rights reserved.

include(FetchContent)
include(CheckLocalSource)

# 在最开始就设置 CMP0063 策略
cmake_policy(SET CMP0063 NEW)

# 设置本地源码路径（需要根据实际路径修改）
set(SRC_NAME "gflags")
message(STATUS "get ${SRC_NAME} ...")


#-----------------step1:检查指定目录下的库是否存在-----------------------#
# 清除 find_package 的缓存
unset(${SRC_NAME}_DIR CACHE)
unset(${SRC_NAME}_FOUND CACHE)

# 下面这句话和find_package中PATHS用一个即可
# set(${SRC_NAME}_DIR "${CMAKE_PREFIX_PATH}/${SRC_NAME}" CACHE PATH "Path to ${SRC_NAME} config file" FORCE)

# 只在指定目录中查找库
find_package(${SRC_NAME} QUIET
        NO_DEFAULT_PATH
        PATHS "${CMAKE_PREFIX_PATH}/${SRC_NAME}")
      
# 打印查找结果
message(STATUS "gflags_FOUND: ${gflags_FOUND}")
message(STATUS "gflags_INCLUDE_DIRS: ${gflags_INCLUDE_DIRS}")
message(STATUS "gflags_LIBRARIES: ${gflags_LIBRARIES}")

if(${SRC_NAME}_FOUND)
    # 检查gflags::gflags目标是否已经存在，如果不存在才创建ALIAS
    if(NOT TARGET gflags::gflags)
        add_library(gflags::gflags ALIAS gflags)
    endif()
    message(STATUS "${SRC_NAME} found via find_package at: ${${SRC_NAME}_DIR}")
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

set(gflags_DOWNLOAD_URL
    "https://github.com/gflags/gflags/archive/v2.2.2.tar.gz"
    CACHE STRING "")

message(STATUS "gflags_LOCAL_SOURCE: ${gflags_LOCAL_SOURCE}")
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

message(STATUS "1111111111111")
message(STATUS "111111gflags_BINARY_DIR = ${gflags_BINARY_DIR}")
# Wrap it in a function to restrict the scope of the variables
function(get_gflags)
message(STATUS "222222gflags_BINARY_DIR = ${gflags_BINARY_DIR}")
  FetchContent_GetProperties(gflags)
  message(STATUS "33333gflags_BINARY_DIR = ${gflags_BINARY_DIR}")
  if(NOT gflags_POPULATED)
    FetchContent_Populate(gflags)
    message(STATUS "4444gflags_BINARY_DIR = ${gflags_BINARY_DIR}")
    message(STATUS "222222222222222222")
    set(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE PATH "Installation directory" FORCE)
    set(INSTALL_HEADERS ON CACHE BOOL "Install header files" FORCE)
    #     message(STATUS "444444444444444CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")
    # 设置 gflags 特定的构建选项
    set(BUILD_TESTING OFF CACHE BOOL "Disable gflags testing" FORCE)
    set(INSTALL_SHARED_LIBS ON CACHE BOOL "Install shared libraries" FORCE)
    set(INSTALL_STATIC_LIBS ON CACHE BOOL "Install static libraries" FORCE)
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
message(STATUS "3333333333333")
    file(WRITE ${gflags_SOURCE_DIR}/CMakeLists.txt "${NEW_CMAKE_CONTENT}")
    # 添加子目录
    message(STATUS "gflags_BINARY_DIR = ${gflags_BINARY_DIR}")

    add_subdirectory(${gflags_SOURCE_DIR} ${gflags_BINARY_DIR})
    message(STATUS "44444444444444444")
    
    # 确保安装目标被包含
    install(TARGETS gflags_static gflags_shared
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib)
    endif()
endfunction()

get_gflags()

# import targets:
# gflags::gflags