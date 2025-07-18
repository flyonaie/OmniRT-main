# 基于共享内存的RPC示例CMakeLists.txt

cmake_minimum_required(VERSION 3.10)
project(shm_rpc_example)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加编译选项
add_compile_options(-Wall -Wextra)

# 查找必要的包
find_package(Threads REQUIRED)

# 使用自定义二进制序列化，不再需要msgpack库

# 添加RPC库源文件
set(RPC_SOURCES
    src/server.cpp
    src/client.cpp
)

# 创建RPC库
add_library(shm_rpc ${RPC_SOURCES})

# 设置包含目录
target_include_directories(shm_rpc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

# 链接线程库
target_link_libraries(shm_rpc PUBLIC Threads::Threads rt)

# # 构建服务端示例
add_executable(server_demo example/rpc_server_demo.cpp)
target_link_libraries(server_demo shm_rpc)

# 构建客户端示例
add_executable(client_demo example/rpc_client_demo.cpp)
target_link_libraries(client_demo shm_rpc)

# 安装规则
# install(TARGETS shm_rpc server_demo client_demo
#     RUNTIME DESTINATION bin
#     LIBRARY DESTINATION lib
#     ARCHIVE DESTINATION lib
# )

# install(FILES 
#     src/common.h
#     src/server.h
#     src/client.h
#     DESTINATION include/omnirt/rpc
# )
