#!/bin/bash

# exit on error and print each command
set -e

cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DAIMRT_INSTALL=ON \
    -DCMAKE_INSTALL_PREFIX=./build/install \
    -DAIMRT_BUILD_TESTS=OFF \
    -DAIMRT_BUILD_EXAMPLES=ON \
    -DAIMRT_BUILD_DOCUMENT=ON \
    -DAIMRT_BUILD_RUNTIME=ON \
    -DAIMRT_BUILD_CLI_TOOLS=ON \
    -DAIMRT_BUILD_PYTHON_RUNTIME=ON \
    -DAIMRT_USE_FMT_LIB=ON \
    -DAIMRT_BUILD_WITH_PROTOBUF=ON \
    -DAIMRT_USE_LOCAL_PROTOC_COMPILER=OFF \
    -DAIMRT_USE_PROTOC_PYTHON_PLUGIN=OFF \
    -DAIMRT_BUILD_WITH_ROS2=ON \
    -DAIMRT_BUILD_NET_PLUGIN=ON \
    -DAIMRT_BUILD_MQTT_PLUGIN=ON \
    -DAIMRT_BUILD_ZENOH_PLUGIN=ON \
    -DAIMRT_BUILD_ICEORYX_PLUGIN=ON \
    -DAIMRT_BUILD_ROS2_PLUGIN=ON \
    -DAIMRT_BUILD_RECORD_PLAYBACK_PLUGIN=ON \
    -DAIMRT_BUILD_TIME_MANIPULATOR_PLUGIN=ON \
    -DAIMRT_BUILD_PARAMETER_PLUGIN=ON \
    -DAIMRT_BUILD_LOG_CONTROL_PLUGIN=ON \
    -DAIMRT_BUILD_OPENTELEMETRY_PLUGIN=ON \
    -DAIMRT_BUILD_GRPC_PLUGIN=ON \
    -DAIMRT_BUILD_ECHO_PLUGIN=ON \
    -DAIMRT_BUILD_PYTHON_PACKAGE=ON \
    $@

if [ -d ./build/install ]; then
    rm -rf ./build/install
fi

cmake --build build --config Release --target install --parallel $(nproc)
