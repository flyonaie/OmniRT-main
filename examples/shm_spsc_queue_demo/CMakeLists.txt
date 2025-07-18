cmake_minimum_required(VERSION 3.10)
project(shm_spsc_queue_demo)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")

# 包含头文件目录
include_directories(
    ${CMAKE_SOURCE_DIR}/../../src
)

# 生产者进程
add_executable(producer producer.cpp)
target_link_libraries(producer rt)

# 消费者进程
add_executable(consumer consumer.cpp)
target_link_libraries(consumer rt)

# 互相通信的示例
add_executable(bidirectional_demo bidirectional_demo.cpp)
target_link_libraries(bidirectional_demo rt pthread)

# 性能测试程序
add_executable(perf_test perf_test.cpp)
target_link_libraries(perf_test rt pthread)
