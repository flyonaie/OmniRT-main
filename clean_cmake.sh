#!/bin/bash

# 设置工程根目录
PROJECT_ROOT=$(pwd)

echo "开始清理CMake文件..."

# 清理build目录，但保留external目录
if [ -d "$PROJECT_ROOT/build" ]; then
    echo "清理build目录..."
    rm -rf "$PROJECT_ROOT/build"
fi

# 清理项目CMake缓存文件，但保留external目录
find "$PROJECT_ROOT" -type d -name "CMakeFiles" ! -path "*/external/*" -exec echo "删除: {}" \; -exec rm -rf {} \;
find "$PROJECT_ROOT" -type f -name "CMakeCache.txt" ! -path "*/external/*" -exec echo "删除: {}" \; -exec rm -rf {} \;
find "$PROJECT_ROOT" -type f -name "cmake_install.cmake" ! -path "*/external/*" -exec echo "删除: {}" \; -exec rm -rf {} \;
find "$PROJECT_ROOT" -type f -name "Makefile" ! -path "*/external/*" -exec echo "删除: {}" \; -exec rm -rf {} \;

echo "清理完成！"
