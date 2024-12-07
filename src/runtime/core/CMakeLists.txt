# Copyright (c) 2023, AgiBot Inc.
# All rights reserved.

set_namespace()

# Get the current folder name
string(REGEX REPLACE ".*/\(.*\)" "\\1" CUR_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Get namespace
get_namespace(CUR_SUPERIOR_NAMESPACE)
message(STATUS "core-CUR_SUPERIOR_NAMESPACE: ${CUR_SUPERIOR_NAMESPACE}")
string(REPLACE "::" "_" CUR_SUPERIOR_NAMESPACE_UNDERLINE ${CUR_SUPERIOR_NAMESPACE})

# Set target name
set(CUR_TARGET_NAME ${CUR_SUPERIOR_NAMESPACE_UNDERLINE}_${CUR_DIR})
set(CUR_TARGET_ALIAS_NAME ${CUR_SUPERIOR_NAMESPACE}::${CUR_DIR})

# Add subdirectories for independent compilation
add_subdirectory(util)
add_subdirectory(allocator)
# add_subdirectory(channel)
# add_subdirectory(configurator)
# add_subdirectory(executor)
# add_subdirectory(logger)
# add_subdirectory(module)
# add_subdirectory(parameter)
# add_subdirectory(plugin)
# add_subdirectory(rpc)


# Set file collection for core directory
file(GLOB head_files ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB src ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)
file(GLOB test_files ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cc)
list(REMOVE_ITEM src ${test_files})

# Add target
add_library(${CUR_TARGET_NAME} STATIC)
add_library(${CUR_TARGET_ALIAS_NAME} ALIAS ${CUR_TARGET_NAME})

# Set source file of target
target_sources(${CUR_TARGET_NAME} 
    PRIVATE ${src}
    PRIVATE $<TARGET_OBJECTS:${CUR_TARGET_NAME}_util>
    PRIVATE $<TARGET_OBJECTS:${CUR_TARGET_NAME}_allocator>
)

# target_sources(${CUR_TARGET_NAME} 
#     PRIVATE ${src}
#     PRIVATE $<TARGET_OBJECTS:${CUR_TARGET_NAME}_allocator>
#     PRIVATE $<TARGET_OBJECTS:${CUR_TARGET_NAME}_channel>
#     PRIVATE $<TARGET_OBJECTS:${CUR_TARGET_NAME}_configurator>
#     PRIVATE $<TARGET_OBJECTS:${CUR_TARGET_NAME}_executor>
#     PRIVATE $<TARGET_OBJECTS:${CUR_TARGET_NAME}_logger>
#     PRIVATE $<TARGET_OBJECTS:${CUR_TARGET_NAME}_module>
#     PRIVATE $<TARGET_OBJECTS:${CUR_TARGET_NAME}_parameter>
#     PRIVATE $<TARGET_OBJECTS:${CUR_TARGET_NAME}_plugin>
#     PRIVATE $<TARGET_OBJECTS:${CUR_TARGET_NAME}_rpc>
#     PRIVATE $<TARGET_OBJECTS:${CUR_TARGET_NAME}_util>)

# Set head files of target
target_sources(${CUR_TARGET_NAME} INTERFACE FILE_SET HEADERS BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} FILES ${head_files})

# Set include path of target
target_include_directories(
  ${CUR_TARGET_NAME}
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
         $<INSTALL_INTERFACE:include/aimrt>)

# Set link libraries of target
target_link_libraries(
  ${CUR_TARGET_NAME}
  PUBLIC ${CMAKE_DL_LIBS}
         asio::asio
         yaml-cpp::yaml-cpp
         TBB::tbb
         aimrt::interface::aimrt_module_cpp_interface
         aimrt::common::util)

# Add -Werror option
include(AddWerror)
add_werror(${CUR_TARGET_NAME})

# Set version of target
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
  add_gtest_target(TEST_TARGET ${CUR_TARGET_NAME} TEST_SRC ${test_files})
endif()
