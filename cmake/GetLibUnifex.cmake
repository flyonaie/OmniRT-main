include(FetchContent)
include(CheckLocalSource)

# 设置本地源码路径（需要根据实际路径修改）
set(SRC_NAME "libunifex")
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
        PATHS "${CMAKE_PREFIX_PATH}/unifex")

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
  REQUIRED_FILES "CMakeLists.txt" "source"
  RESULT_VAR "${SRC_NAME}_LOCAL_SOURCE"
)

set(libunifex_DOWNLOAD_URL
    "https://github.com/facebookexperimental/libunifex/archive/591ec09e7d51858ad05be979d4034574215f5971.tar.gz"
    CACHE STRING "")

if(libunifex_LOCAL_SOURCE)
  FetchContent_Declare(
    libunifex
    SOURCE_DIR ${libunifex_LOCAL_SOURCE}
    OVERRIDE_FIND_PACKAGE)
else()
  message(STATUS "downloading libunifex from ${libunifex_DOWNLOAD_URL}")
  FetchContent_Declare(
    libunifex
    URL ${libunifex_DOWNLOAD_URL}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    OVERRIDE_FIND_PACKAGE)
endif()

FetchContent_GetProperties(libunifex)
if(NOT libunifex_POPULATED)
  set(UNIFEX_BUILD_EXAMPLES
      OFF
      CACHE BOOL "")

  FetchContent_MakeAvailable(libunifex)

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(unifex PRIVATE -Wno-unused-but-set-variable)
  endif()

  if(NOT TARGET unifex::unifex)
    add_library(unifex::unifex ALIAS unifex)
  endif()
endif()

# import targets:
# unifex::unifex
