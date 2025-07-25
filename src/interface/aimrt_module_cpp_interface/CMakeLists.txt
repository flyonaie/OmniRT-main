# Copyright (c) 2023, AgiBot Inc.
# All rights reserved.

# Get the current folder name
string(REGEX REPLACE ".*/\(.*\)" "\\1" CUR_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Get namespace
get_namespace(CUR_SUPERIOR_NAMESPACE)
string(REPLACE "::" "_" CUR_SUPERIOR_NAMESPACE_UNDERLINE ${CUR_SUPERIOR_NAMESPACE})

# Set target name
set(CUR_TARGET_NAME ${CUR_SUPERIOR_NAMESPACE_UNDERLINE}_${CUR_DIR})
set(CUR_TARGET_ALIAS_NAME ${CUR_SUPERIOR_NAMESPACE}::${CUR_DIR})

# Set file collection
# new code
file(GLOB head_files ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE subdir_head_files
  ${CMAKE_CURRENT_SOURCE_DIR}/util/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/allocator/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/configurator/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/executor/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/logger/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/channel/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/rpc/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/module/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/core.h
  ${CMAKE_CURRENT_SOURCE_DIR}/module_base.h
  )
list(APPEND head_files ${subdir_head_files})

file(GLOB_RECURSE test_files 
  ${CMAKE_CURRENT_SOURCE_DIR}/allocator/*_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/util/*_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/configurator/*_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/executor/*_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/logger/*_test.cc
  # ${CMAKE_CURRENT_SOURCE_DIR}/parameter/*_test.cc
  )

# old code
# file(GLOB_RECURSE head_files ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
# file(GLOB_RECURSE test_files ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cc)
# list(FILTER head_files EXCLUDE REGEX ".*/co/.*")
# list(FILTER test_files EXCLUDE REGEX ".*/co/.*")


# Add target
add_library(${CUR_TARGET_NAME} INTERFACE)
add_library(${CUR_TARGET_ALIAS_NAME} ALIAS ${CUR_TARGET_NAME})

# Set include path of target
target_include_directories(
  ${CUR_TARGET_NAME}
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            $<INSTALL_INTERFACE:include/aimrt>)

# Set head files of target
target_sources(${CUR_TARGET_NAME} INTERFACE FILE_SET HEADERS BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} FILES ${head_files})

message("CUR_TARGET_NAME: ${CUR_TARGET_NAME}")
# Set link libraries of target
target_link_libraries(
  ${CUR_TARGET_NAME}
  INTERFACE aimrt::common::util
            aimrt::interface::aimrt_module_c_interface)

# Executor
if(AIMRT_EXECUTOR_USE_STDEXEC)
  target_link_libraries(${CUR_TARGET_NAME} INTERFACE STDEXEC::stdexec)
  target_compile_definitions(${CUR_TARGET_NAME} INTERFACE AIMRT_EXECUTOR_USE_STDEXEC)
else()
  # 避免在导出时直接依赖unifex
  target_compile_definitions(${CUR_TARGET_NAME} INTERFACE AIMRT_USE_UNIFEX)
  if(NOT AIMRT_INSTALL)
    target_link_libraries(${CUR_TARGET_NAME} INTERFACE unifex::unifex)
  endif()
endif()

# Set installation of target
if(AIMRT_INSTALL)
  set_property(TARGET ${CUR_TARGET_NAME} PROPERTY EXPORT_NAME ${CUR_TARGET_ALIAS_NAME})
  install(
    TARGETS ${CUR_TARGET_NAME}
    EXPORT ${INSTALL_CONFIG_NAME}
    FILE_SET HEADERS
    DESTINATION include/aimrt/${CUR_DIR})
endif()

message("CPPINF-UTIL-AIMRT_BUILD_TESTS: ${AIMRT_BUILD_TESTS}")
# Set test of target
if(AIMRT_BUILD_TESTS AND test_files)
  message("CPPINF-UTIL-CUR_TARGET_NAME: ${CUR_TARGET_NAME}")
  add_gtest_target(
    TEST_TARGET ${CUR_TARGET_NAME}
    TEST_SRC ${test_files})
endif()
