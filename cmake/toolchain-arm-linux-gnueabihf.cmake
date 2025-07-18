set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Specify the cross compiler
# set(TOOLCHAIN_PATH /usr/local/workspace/tools/gcc-linaro-11.3.1-2022.06-x86_64_arm-linux-gnueabihf)
# set(CMAKE_C_COMPILER ${TOOLCHAIN_PATH}/bin/arm-linux-gnueabihf-gcc)
# set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PATH}/bin/arm-linux-gnueabihf-g++)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# Set search paths
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 设置编译器标志
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard")

# Force rebuild all dependencies for cross compilation
set(AIMRT_FORCE_REBUILD_DEPS ON)
set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/install_arm)

# Clear find package cache to force rebuilding dependencies
unset(fmt_FOUND CACHE)
unset(gflags_FOUND CACHE)
unset(yaml-cpp_FOUND CACHE)
unset(asio_FOUND CACHE)
unset(libunifex_FOUND CACHE)

# Force local build for all dependencies
set(AIMRT_USE_LOCAL_FMT ON CACHE BOOL "" FORCE)
set(AIMRT_USE_LOCAL_GFLAGS ON CACHE BOOL "" FORCE)
set(AIMRT_USE_LOCAL_YAML_CPP ON CACHE BOOL "" FORCE)
set(AIMRT_USE_LOCAL_ASIO ON CACHE BOOL "" FORCE)
set(ASIO_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/_deps/asio-src/asio/include)
include_directories(${ASIO_INCLUDE_DIR})
set(AIMRT_USE_LOCAL_UNIFEX ON CACHE BOOL "" FORCE)

