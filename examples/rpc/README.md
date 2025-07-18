# 基于共享内存的RPC框架

这是一个基于共享内存实现的高性能RPC（远程过程调用）框架。它利用OmniRT的共享内存无锁队列组件，实现了跨进程的方法调用功能，具有低延迟和高吞吐量的特点。

## 特性

- **高性能**：基于共享内存和无锁队列实现，避免了网络通信和序列化的开销
- **简单易用**：接口设计简洁明了，支持同步和异步调用
- **类型安全**：支持多种参数和返回值类型，提供类型检查
- **无锁设计**：服务端和客户端各自维护独立的请求/响应队列，采用单生产者-单消费者模式
- **跨进程**：支持跨进程通信，便于实现分布式系统和微服务架构

## 架构设计

RPC框架主要包含以下组件：

1. **共享内存队列**：基于`ShmBoundedSpscLockfreeQueue`实现的高性能消息队列
2. **RPC服务端**：负责注册和处理RPC方法
3. **RPC客户端**：提供同步和异步调用接口
4. **消息格式**：定义了RPC通信的消息结构和序列化方式

每个RPC通道由两个共享内存队列组成：
- **请求队列**：客户端写入请求，服务端读取请求
- **响应队列**：服务端写入响应，客户端读取响应

## 使用示例

### 服务端示例

```cpp
#include "rpc/server.h"

// 定义RPC方法
int Add(int a, int b) {
    return a + b;
}

int main() {
    // 创建RPC服务器
    omnirt::rpc::Server server("/rpc_demo");
    
    // 注册RPC方法
    server.Bind("add", &Add);
    
    // 也可以绑定lambda函数
    server.Bind("square", [](int x) -> int {
        return x * x;
    });
    
    // 运行服务器（阻塞）
    server.Run();
    
    return 0;
}
```

### 客户端示例

```cpp
#include "rpc/client.h"

int main() {
    // 创建RPC客户端
    omnirt::rpc::Client client("/rpc_demo");
    
    // 等待连接
    client.WaitForConnection();
    
    // 同步调用
    int result = client.Call<int>("add", 10, 20);
    std::cout << "10 + 20 = " << result << std::endl;
    
    // 异步调用
    auto future = client.AsyncCall<int, int>("square", 12);
    
    // 做其他工作...
    
    // 获取异步结果
    if (future->Wait(std::chrono::milliseconds(1000))) {
        int square_result = std::get<int>(future->GetResult());
        std::cout << "12^2 = " << square_result << std::endl;
    }
    
    return 0;
}
```

## 编译和运行

1. 编译项目：
```bash
mkdir build && cd build
cmake ..
make
```

2. 运行示例：
```bash
# 终端1：运行服务端
./server_demo

# 终端2：运行客户端
./client_demo
```

## 设计考量

- **无锁设计**：通过使用`ShmBoundedSpscLockfreeQueue`实现无锁通信，避免了锁的开销和潜在的死锁问题
- **共享内存**：通过共享内存实现跨进程通信，避免了网络通信的开销和序列化/反序列化的成本
- **异步支持**：提供异步调用接口，允许客户端并发发送多个请求
- **错误处理**：完善的错误处理机制，包括超时处理、异常传递和错误码

## 局限性

- 当前仅支持在同一主机上的进程间通信，不支持网络通信
- 参数和返回值类型有限，不支持任意复杂类型的传输
- 服务发现和负载均衡功能尚未实现

## 未来计划

- 添加服务注册和发现功能
- 支持更多参数和返回值类型
- 添加安全认证机制
- 实现负载均衡和故障转移
