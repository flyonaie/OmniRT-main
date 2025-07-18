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
file(GLOB_RECURSE head_files 
    ${CMAKE_CURRENT_SOURCE_DIR}/atomic_hash_map.h    
    ${CMAKE_CURRENT_SOURCE_DIR}/block_queue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/buffer_util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bounded_spsc_lockfree_queue.h

    ${CMAKE_CURRENT_SOURCE_DIR}/deferred.h
    ${CMAKE_CURRENT_SOURCE_DIR}/exception.h
    ${CMAKE_CURRENT_SOURCE_DIR}/format.h
    ${CMAKE_CURRENT_SOURCE_DIR}/futex_atomic.h

    ${CMAKE_CURRENT_SOURCE_DIR}/light_signal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/log_util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/macros.h
    ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json_util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stl_tool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/string_util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/time_util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/url_encode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/url_parser.h
    )

file(GLOB_RECURSE test_files 
    ${CMAKE_CURRENT_SOURCE_DIR}/block_queue_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/bounded_spsc_lockfree_queue_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/buffer_util_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/deferred_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/light_signal_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/log_util_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/macros_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/stl_tool_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/string_util_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/time_util_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/url_encode_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/url_parser_test.cc
    )
# file(GLOB_RECURSE head_files ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
# file(GLOB_RECURSE test_files ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cc)

# Add target
message("CUR_TARGET_NAME: ${CUR_TARGET_NAME}")
add_library(${CUR_TARGET_NAME} INTERFACE)
add_library(${CUR_TARGET_ALIAS_NAME} ALIAS ${CUR_TARGET_NAME})

# Set include path of target
target_include_directories(
  ${CUR_TARGET_NAME}
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            $<INSTALL_INTERFACE:include/aimrt>)

# Set head files of target
target_sources(${CUR_TARGET_NAME} INTERFACE FILE_SET HEADERS BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} FILES ${head_files})

# Set link libraries of target
# target_link_libraries(${CUR_TARGET_NAME} INTERFACE xxx)

# Fmt lib
if(AIMRT_USE_FMT_LIB)
  target_link_libraries(${CUR_TARGET_NAME} INTERFACE fmt::fmt-header-only)
  target_compile_definitions(${CUR_TARGET_NAME} INTERFACE AIMRT_USE_FMT_LIB)
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

# Set test of target
if(AIMRT_BUILD_TESTS AND test_files)
  message("COMMON-UTIL-CUR_TARGET_NAME: ${CUR_TARGET_NAME}")
  add_gtest_target(TEST_TARGET ${CUR_TARGET_NAME} 
  TEST_SRC ${test_files})
endif()
