#!/bin/bash

# 设置错误退出
set -e

# 显示使用帮助
show_usage() {
    echo "用法: $0 [选项]"
    echo "选项:"
    echo "  --x86         仅编译X86平台"
    echo "  --arm         仅编译ARM平台"
    echo "  --all         同时编译X86和ARM平台 (默认)"
    echo "  -h, --help    显示此帮助信息"
}

# 默认编译所有平台
BUILD_X86=true
BUILD_ARM=true

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --x86)
            BUILD_X86=true
            BUILD_ARM=false
            shift
            ;;
        --arm)
            BUILD_X86=false
            BUILD_ARM=true
            shift
            ;;
        --all)
            BUILD_X86=true
            BUILD_ARM=true
            shift
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            echo "错误: 未知选项 $1"
            show_usage
            exit 1
            ;;
    esac
done

echo "===== 开始编译工程 ====="

# 切换到上级目录
cd ..

# 编译X86平台
build_x86() {
    echo "===== 编译x86平台版本 ====="
    rm -rf build_x86 install_x86
    mkdir -p build_x86
    mkdir -p install_x86
    cd build_x86

    cmake .. -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_CROSS_COMPILATION=OFF \
        -DAIMRT_INSTALL=ON \
        -DCMAKE_INSTALL_PREFIX=../install_x86

    make -j$(nproc)
    make install
    cd ..

    echo "x86平台库已编译完成，安装在 ./install_x86"
}

# 编译ARM平台
build_arm() {
    echo "===== 编译ARM平台版本 ====="
    rm -rf build_arm install_arm
    mkdir -p build_arm
    cd build_arm

    # 确保使用正确的ARM交叉编译工具链
    # 如果需要，取消下面的注释并设置正确的路径
    # export PATH=/usr/local/workspace/tools/gcc-linaro-11.3.1-2022.06-x86_64_arm-linux-gnueabihf/bin:$PATH

    cmake .. -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_CROSS_COMPILATION=ON \
        -DAIMRT_INSTALL=ON \
        -DCMAKE_INSTALL_PREFIX=../install_arm

    make -j$(nproc)
    make install
    cd ..

    echo "ARM平台库已编译完成，安装在 ./install_arm"
}

# 根据选择执行编译
if [ "$BUILD_X86" = true ]; then
    build_x86
fi

if [ "$BUILD_ARM" = true ]; then
    build_arm
fi

# 显示编译结果摘要
echo "===== 编译结果摘要 ====="
if [ "$BUILD_X86" = true ]; then
    echo "x86平台库位置: ./install_x86"
fi
if [ "$BUILD_ARM" = true ]; then
    echo "ARM平台库位置: ./install_arm"
fi
echo "===== 编译完成 ====="
