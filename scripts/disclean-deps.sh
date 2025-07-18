#!/bin/bash

# 设置工程根目录
PROJECT_ROOT="$(pwd)/../"
SOURCE_DIR="${PROJECT_ROOT}/_deps"

echo "开始清理第三方库构建文件..."

# 查找并删除所有 *-build 目录
find "${SOURCE_DIR}" -maxdepth 1 -type d -name "*-build" -exec echo "删除: {}" \; -exec rm -rf {} \;

# 查找并删除所有 *-subbuild 目录
find "${SOURCE_DIR}" -maxdepth 1 -type d -name "*-subbuild" -exec echo "删除: {}" \; -exec rm -rf {} \;

echo "清理完成！"