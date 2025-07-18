# Copyright (c) 2023, AgiBot Inc.
# All rights reserved.

include(FetchContent)
include(CheckLocalSource)

# 设置本地源码路径（需要根据实际路径修改）
set(SRC_NAME "nlohmann_json")
message(STATUS "get ${SRC_NAME} ...")

#-----------------step1:检查指定目录下的库是否存在-----------------------#
# 清除 find_package 的缓存
unset(${SRC_NAME}_DIR CACHE)
unset(${SRC_NAME}_FOUND CACHE)

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
check_local_source(
    SRC_NAME "${SRC_NAME}"
    SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/${SRC_NAME}-src"
    REQUIRED_FILES "CMakeLists.txt" "single_include"
    RESULT_VAR "${SRC_NAME}_LOCAL_SOURCE"
)

set(nlohmann_json_DOWNLOAD_URL
    "https://github.com/nlohmann/json/archive/v3.11.3.tar.gz"
    CACHE STRING "")

#-----------------step3:开启下载或者构建-----------------------#
if(nlohmann_json_LOCAL_SOURCE)
  FetchContent_Declare(
    nlohmann_json
    SOURCE_DIR ${nlohmann_json_LOCAL_SOURCE}
    OVERRIDE_FIND_PACKAGE)
else()
  FetchContent_Declare(
    nlohmann_json
    URL ${nlohmann_json_DOWNLOAD_URL}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    OVERRIDE_FIND_PACKAGE)
endif()

FetchContent_GetProperties(nlohmann_json)
if(NOT nlohmann_json_POPULATED)
  FetchContent_MakeAvailable(nlohmann_json)
endif()

# 创建接口库
if(NOT TARGET nlohmann_json_interface)
  add_library(nlohmann_json_interface INTERFACE)
  add_library(nlohmann_json::interface ALIAS nlohmann_json_interface)

  # 收集头文件
  file(GLOB_RECURSE nlohmann_json_headers 
       ${nlohmann_json_SOURCE_DIR}/single_include/*.hpp)
       
  # 添加头文件到库
  target_sources(nlohmann_json_interface INTERFACE 
                FILE_SET HEADERS 
                BASE_DIRS ${nlohmann_json_SOURCE_DIR}/single_include 
                FILES ${nlohmann_json_headers})
                
  # 设置包含路径
  target_include_directories(nlohmann_json_interface INTERFACE
      $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/single_include>
      $<INSTALL_INTERFACE:include>)
      
  # 配置安装
  set_property(TARGET nlohmann_json_interface PROPERTY EXPORT_NAME nlohmann_json::interface)
  install(
    TARGETS nlohmann_json_interface
    EXPORT nlohmann_json-config
    FILE_SET HEADERS
    DESTINATION include)
    
  install(EXPORT nlohmann_json-config DESTINATION lib/cmake/nlohmann_json)
endif()

# import targets:
# nlohmann_json::nlohmann_json
