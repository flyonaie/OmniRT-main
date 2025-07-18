cmake_minimum_required(VERSION 3.10)
project(spsc_queue_example)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找线程库
find_package(Threads REQUIRED)

# 包含头文件路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src)

# 添加可执行文件
add_executable(basic_example basic_example.cpp)
add_executable(data_processing_example data_processing_example.cpp)
add_executable(latency_benchmark latency_benchmark.cpp)

# 链接线程库
target_link_libraries(basic_example PRIVATE Threads::Threads)
target_link_libraries(data_processing_example PRIVATE Threads::Threads)
target_link_libraries(latency_benchmark PRIVATE Threads::Threads)

# 启用优化
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
