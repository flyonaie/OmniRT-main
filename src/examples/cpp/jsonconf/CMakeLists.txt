# Copyright (c) 2023, AgiBot Inc.
# All rights reserved.

# Get the current folder name
string(REGEX REPLACE ".*/\(.*\)" "\\1" CUR_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Set namespace
set_namespace()

# module
add_subdirectory(module/jsonconf_module)

# pkg
add_subdirectory(pkg/jsonconf_pkg)

# app
# add_subdirectory(app/jsonconf_app)

# install
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(CUR_INSTALL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/install/linux)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(CUR_INSTALL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/install/win)
else()
  message(FATAL_ERROR "Unsupport os")
endif()

# build all
# get_namespace(CUR_SUPERIOR_NAMESPACE)
# string(REPLACE "::" "_" CUR_SUPERIOR_NAMESPACE_UNDERLINE ${CUR_SUPERIOR_NAMESPACE})
# add_custom_target(
#   ${CUR_SUPERIOR_NAMESPACE_UNDERLINE}_${CUR_DIR}_build_all ALL
#   COMMAND ${CMAKE_COMMAND} -E copy_directory ${CUR_INSTALL_SOURCE_DIR}/bin ${CMAKE_BINARY_DIR}
#   DEPENDS aimrt::runtime::main
#           ${CUR_SUPERIOR_NAMESPACE}::${CUR_DIR}::helloworld_app
#           ${CUR_SUPERIOR_NAMESPACE}::${CUR_DIR}::helloworld_pkg)
