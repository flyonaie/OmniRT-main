include(FetchContent)
include(CheckLocalSource)

# 设置本地源码路径（需要根据实际路径修改）
set(SRC_NAME "protobuf")
message(STATUS "get ${SRC_NAME} print ...")

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

# 修改前
# find_package(${SRC_NAME} QUIET
#         NO_DEFAULT_PATH
#         PATHS "${CMAKE_PREFIX_PATH}/${SRC_NAME}")

# 修改后：添加 NO_CMAKE_SYSTEM_PATH
find_package(${SRC_NAME} QUIET
        NO_DEFAULT_PATH
        NO_CMAKE_SYSTEM_PATH  # 禁止搜索系统路径
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

set(protobuf_DOWNLOAD_URL
    "https://github.com/protocolbuffers/protobuf/archive/v3.21.12.tar.gz"
    CACHE STRING "")

if(protobuf_LOCAL_SOURCE)
  FetchContent_Declare(
    protobuf
    SOURCE_DIR ${protobuf_LOCAL_SOURCE}
    OVERRIDE_FIND_PACKAGE)
else()
  FetchContent_Declare(
    protobuf
    URL ${protobuf_DOWNLOAD_URL}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    OVERRIDE_FIND_PACKAGE)
endif()

# Wrap it in a function to restrict the scope of the variables
function(get_protobuf)
  FetchContent_GetProperties(protobuf)
  if(NOT protobuf_POPULATED)
    set(protobuf_BUILD_TESTS OFF)

    set(protobuf_BUILD_CONFORMANCE OFF)

    set(protobuf_DISABLE_RTTI OFF)

    set(protobuf_WITH_ZLIB OFF)

    set(protobuf_MSVC_STATIC_RUNTIME OFF)

    set(protobuf_INSTALL ${AIMRT_INSTALL})

    set(protobuf_VERBOSE ON)
  
    set(protobuf_BUILD_SHARED_LIBS ON)

    # 禁用abseil-cpp依赖
    # set(protobuf_ABSL_PROVIDER OFF)

    # 禁用utf8_range测试，避免依赖abseil-cpp
    # set(utf8_range_ENABLE_TESTS OFF)

    # 在FetchContent_Populate之前设置编译选项
    # 这将在下载之前就设置了环境变量
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-attributes")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-attributes")
    
    # 后续处理
    FetchContent_MakeAvailable(protobuf)
    
  # 确保 protoc 可执行文件被正确导入
  if(TARGET protoc AND NOT TARGET protobuf::protoc)
    message(STATUS "找到 protoc 目标，创建 protobuf::protoc 别名")
    add_executable(protobuf::protoc ALIAS protoc)
  endif()
    # 此外再单独设置各目标的编译选项
    target_compile_options(libprotobuf PRIVATE -Wno-attributes)
    target_compile_options(libprotoc PRIVATE -Wno-attributes)
    target_compile_options(libprotobuf-lite PRIVATE -Wno-attributes)
    
    
#   set_target_properties(libprotobuf PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY "${THIRD_PARTY_DEPS_DIR}/protobuf/bin"
#     LIBRARY_OUTPUT_DIRECTORY "${THIRD_PARTY_DEPS_DIR}/protobuf/lib"
#     ARCHIVE_OUTPUT_DIRECTORY "${THIRD_PARTY_DEPS_DIR}/protobuf/lib"
#   )
  endif()
endfunction()

get_protobuf()

# import targets:
# protobuf::libprotobuf
# protobuf::libprotobuf-lite
# protobuf::libprotoc
# protobuf::protoc
