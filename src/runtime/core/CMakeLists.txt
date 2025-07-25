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
message(STATUS "CUR_TARGET_NAME:----- ${CUR_TARGET_NAME}")
message(STATUS "CUR_TARGET_ALIAS_NAME:----- ${CUR_TARGET_ALIAS_NAME}")

# Set file collection
message(STATUS "AimRT version:----- ${AIMRT_VERSION}")
file(GLOB_RECURSE head_files 
    ${CMAKE_CURRENT_SOURCE_DIR}/util/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/allocator/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/configurator/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/executor/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/logger/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/channel/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rpc/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/module/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aimrt_core.h
    ${CMAKE_CURRENT_SOURCE_DIR}/main/core_main.h
    )
file(GLOB_RECURSE src 
    ${CMAKE_CURRENT_SOURCE_DIR}/util/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/allocator/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/configurator/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/executor/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/logger/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/channel/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/rpc/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/module/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/aimrt_core.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/main/core_main.cc
    )
file(GLOB_RECURSE test_files
    ${CMAKE_CURRENT_SOURCE_DIR}/util/*_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/allocator/*_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/configurator/*_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/executor/*_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/logger/*_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/channel/*_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/rpc/*_test.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/module/*_test.cc
    # ${CMAKE_CURRENT_SOURCE_DIR}/aimrt_core_test.cc
    )

if(NOT AIMRT_EXECUTOR_USE_STDEXEC)
  # list(REMOVE_ITEM src ${CMAKE_CURRENT_SOURCE_DIR}/executor/asio_strand_executor.cc)
  # list(REMOVE_ITEM head_files ${CMAKE_CURRENT_SOURCE_DIR}/executor/asio_strand_executor.h)
  # list(REMOVE_ITEM src ${CMAKE_CURRENT_SOURCE_DIR}/executor/asio_thread_executor.cc)
  # list(REMOVE_ITEM head_files ${CMAKE_CURRENT_SOURCE_DIR}/executor/asio_thread_executor.h)
  # list(REMOVE_ITEM test_files ${CMAKE_CURRENT_SOURCE_DIR}/executor/asio_thread_executor_test.cc)
endif()

list(REMOVE_ITEM src ${test_files})
list(REMOVE_ITEM src ${CMAKE_CURRENT_SOURCE_DIR}/executor/tbb_thread_executor.cc)
list(REMOVE_ITEM head_files ${CMAKE_CURRENT_SOURCE_DIR}/executor/tbb_thread_executor.h)
list(REMOVE_ITEM test_files ${CMAKE_CURRENT_SOURCE_DIR}/executor/tbb_thread_executor_test.cc)

# Add library
add_library(${CUR_TARGET_NAME} SHARED)
add_library(${CUR_TARGET_ALIAS_NAME} ALIAS ${CUR_TARGET_NAME})
message(STATUS "CUR_TARGET_NAME:----- ${CUR_TARGET_NAME}")
message(STATUS "CUR_TARGET_ALIAS_NAME:----- ${CUR_TARGET_ALIAS_NAME}")

# Set source file of target
target_sources(${CUR_TARGET_NAME} PRIVATE ${src})

# Set head files of target
target_sources(${CUR_TARGET_NAME} INTERFACE FILE_SET HEADERS BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} FILES ${head_files})

# Set include path of target
message(STATUS "-----CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(
  ${CUR_TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
         $<INSTALL_INTERFACE:include/aimrt>)

# Set link library of target
target_link_libraries(
  ${CUR_TARGET_NAME}
  PUBLIC ${CMAKE_DL_LIBS}
    asio::asio
         yaml-cpp::yaml-cpp
        #  TBB::tbbmalloc
        #  TBB::tbbmalloc_proxy
        #  TBB::tbb
    aimrt::interface::aimrt_module_cpp_interface
        #  aimrt::interface::aimrt_core_plugin_interface
    aimrt::common::util)

target_compile_options(${CUR_TARGET_NAME}
    PRIVATE
      -fPIC
      -fvisibility=default)

# Add -Werror option
include(AddWerror)
add_werror(${CUR_TARGET_NAME})

# # Set version of target
target_compile_definitions(${CUR_TARGET_NAME} PUBLIC AIMRT_VERSION="${AIMRT_VERSION}")

# Set installation of target
if(AIMRT_INSTALL)
  set_property(TARGET ${CUR_TARGET_NAME} PROPERTY EXPORT_NAME ${CUR_TARGET_ALIAS_NAME})
  install(
    TARGETS ${CUR_TARGET_NAME}
    EXPORT ${INSTALL_CONFIG_NAME}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
            FILE_SET HEADERS
            DESTINATION include/aimrt/${CUR_DIR})
endif()

# Set test of target
if(AIMRT_BUILD_TESTS AND test_files)
  message("RUNTIME-CORE-CUR_TARGET_NAME: ${CUR_TARGET_NAME}")
  add_gtest_target(TEST_TARGET ${CUR_TARGET_NAME} TEST_SRC ${test_files})
endif()
