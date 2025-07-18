# Copyright (c) 2023, AgiBot Inc.
# All rights reserved.
# set_root_namespace("aimrt")
# Get the current folder name
string(REGEX REPLACE ".*/\(.*\)" "\\1" CUR_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Set namespace
set_namespace()

# module
add_subdirectory(hello)