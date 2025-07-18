include(FetchContent)
include(CheckLocalSource)

# 设置本地源码路径（需要根据实际路径修改）
set(SRC_NAME "fmt")
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
    REQUIRED_FILES "CMakeLists.txt" "src"
    RESULT_VAR "${SRC_NAME}_LOCAL_SOURCE"
)

set(fmt_DOWNLOAD_URL 
    https://github.com/fmtlib/fmt/archive/10.2.1.tar.gz
    CACHE STRING "")

message(STATUS "fmt_LOCAL_SOURCE: ${fmt_LOCAL_SOURCE}")
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

#-----------------step3:开启下载或者构建-----------------------#
FetchContent_GetProperties(fmt)
message(STATUS "fmt_POPULATED: ${fmt_POPULATED}")
if(NOT fmt_POPULATED) 
    # 获取并构建fmt
    message(STATUS "开始下载或构建fmt...")    

    FetchContent_MakeAvailable(fmt)
   
    message(STATUS "fmt配置文件生成在: ${FMT_INSTALL_PREFIX}/lib/cmake/fmt")
else()
    message(STATUS "fmt已经下载和配置")
endif()

# import targets：
# fmt::fmt