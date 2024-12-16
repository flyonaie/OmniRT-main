# Copyright (c) 2023, AgiBot Inc.
# All rights reserved.

include(FetchContent)
include(CheckLocalSource)

# 设置本地源码路径（需要根据实际路径修改）
set(SRC_NAME "googletest")
message(STATUS "get googletest ...")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
# set(googletest_LOCAL_SOURCE "${CMAKE_SOURCE_DIR}/_deps/googletest-src" CACHE PATH "Path to local googletest source")

# 检查本地源码目录是否存在且包含必要文件
check_local_source(
  SRC_NAME "${SRC_NAME}"
  SOURCE_DIR "_deps/${SRC_NAME}-src"
  REQUIRED_FILES "CMakeLists.txt" "src"
  RESULT_VAR "${SRC_NAME}_LOCAL_SOURCE"
)

set(googletest_DOWNLOAD_URL
    "https://github.com/google/googletest/archive/v1.13.0.tar.gz"
    CACHE STRING "")

if(googletest_LOCAL_SOURCE)
  FetchContent_Declare(
    googletest
    SOURCE_DIR ${googletest_LOCAL_SOURCE}
    OVERRIDE_FIND_PACKAGE)
else()
  FetchContent_Declare(
    googletest
    URL ${googletest_DOWNLOAD_URL}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    OVERRIDE_FIND_PACKAGE)
endif()

FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
  if(WIN32)
    set(gtest_force_shared_crt
        ON
        CACHE BOOL "")
  endif()
  set(INSTALL_GTEST
      OFF
      CACHE BOOL "")

  FetchContent_MakeAvailable(googletest)
endif()

# import targets:
# GTest::gtest
# GTest::gtest_main
# GTest::gmock
# GTest::gmock_main

# add xxxlib_test target for xxxlib
function(add_gtest_target)
  message(STATUS "add_gtest_target")
  cmake_parse_arguments(ARG "" "TEST_TARGET" "TEST_SRC;INC_DIR" ${ARGN})
  set(TEST_TARGET_NAME ${ARG_TEST_TARGET}_test)

  get_target_property(TEST_TARGET_TYPE ${ARG_TEST_TARGET} TYPE)
  message(STATUS "TEST_TARGET_TYPE: ${TEST_TARGET_TYPE}")
  if(${TEST_TARGET_TYPE} STREQUAL SHARED_LIBRARY)
    message(STATUS "SHARED_LIBRARY: ${SHARED_LIBRARY}")
    get_target_property(TEST_TARGET_SOURCES ${ARG_TEST_TARGET} SOURCES)
    add_executable(${TEST_TARGET_NAME} ${ARG_TEST_SRC} ${TEST_TARGET_SOURCES})

    get_target_property(TEST_TARGET_INCLUDE_DIRECTORIES ${ARG_TEST_TARGET} INCLUDE_DIRECTORIES)
    target_include_directories(${TEST_TARGET_NAME} PRIVATE ${ARG_INC_DIR} ${TEST_TARGET_INCLUDE_DIRECTORIES})

    get_target_property(TEST_TARGET_LINK_LIBRARIES ${ARG_TEST_TARGET} LINK_LIBRARIES)
    target_link_libraries(${TEST_TARGET_NAME} PRIVATE ${TEST_TARGET_LINK_LIBRARIES} GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
  else()
    message(STATUS "STATIC_LIBRARY: ${STATIC_LIBRARY}")
    add_executable(${TEST_TARGET_NAME} ${ARG_TEST_SRC})
    target_include_directories(${TEST_TARGET_NAME} PRIVATE ${ARG_INC_DIR})
    target_link_libraries(${TEST_TARGET_NAME} PRIVATE ${ARG_TEST_TARGET} GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
  endif()

  add_test(NAME ${TEST_TARGET_NAME} COMMAND $<TARGET_FILE:${TEST_TARGET_NAME}>)
endfunction()
