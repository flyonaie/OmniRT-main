# CheckLocalSource.cmake
# 用于检查本地源码目录是否存在且包含必要文件
#
# 使用方法:
# include(CheckLocalSource)
# check_local_source(
#   SRC_NAME "your_source_name"
#   SOURCE_DIR "_deps/${SRC_NAME}-src"
#   REQUIRED_FILES "CMakeLists.txt" "src"
#   RESULT_VAR "your_source_LOCAL_SOURCE"
# )

function(check_local_source)
  # 解析参数
  set(options "")
  set(oneValueArgs SRC_NAME SOURCE_DIR RESULT_VAR)
  set(multiValueArgs REQUIRED_FILES)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # 检查必要参数
  if(NOT ARG_SRC_NAME)
    message(FATAL_ERROR "SRC_NAME is required")
  endif()
  if(NOT ARG_SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
  endif()
  if(NOT ARG_RESULT_VAR)
    message(FATAL_ERROR "RESULT_VAR is required")
  endif()

  # 设置本地源码目录路径
  set(LOCAL_SOURCE_DIR "${CMAKE_SOURCE_DIR}/${ARG_SOURCE_DIR}")
  
  # 检查目录是否存在
  set(SOURCE_VALID TRUE)
  if(NOT EXISTS "${LOCAL_SOURCE_DIR}" OR NOT IS_DIRECTORY "${LOCAL_SOURCE_DIR}")
    set(SOURCE_VALID FALSE)
  endif()

  # 检查必要文件
  foreach(required_file ${ARG_REQUIRED_FILES})
    if(NOT EXISTS "${LOCAL_SOURCE_DIR}/${required_file}")
      set(SOURCE_VALID FALSE)
      break()
    endif()
  endforeach()

  # 设置结果
  if(SOURCE_VALID)
    set(${ARG_RESULT_VAR} "${LOCAL_SOURCE_DIR}" CACHE PATH "Path to local ${ARG_SRC_NAME} source")
    message(STATUS "Found local ${ARG_SRC_NAME} source at: ${${ARG_RESULT_VAR}}")
  else()
    set(${ARG_RESULT_VAR} "" CACHE PATH "Path to local ${ARG_SRC_NAME} source")
    message(STATUS "Local ${ARG_SRC_NAME} source not found or incomplete, will download from remote")
  endif()

  # 将结果传递给父作用域
  set(${ARG_RESULT_VAR} "${${ARG_RESULT_VAR}}" PARENT_SCOPE)
endfunction()
