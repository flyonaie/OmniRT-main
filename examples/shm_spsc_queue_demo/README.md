# 共享内存无锁队列示例程序

本示例展示了如何使用 `ShmBoundedSpscLockfreeQueue` 类在不同进程间进行高效的数据通信。该队列基于共享内存实现，使用无锁设计提供高性能的进程间通信机制。

## 功能特性

- **无锁设计**: 使用原子操作和内存序确保线程安全，避免锁开销
- **共享内存**: 支持跨进程通信，进程间直接共享内存区域
- **单生产者-单消费者模式**: 针对SPSC (Single Producer Single Consumer) 场景优化
- **高性能**: 低延迟、高吞吐量，适合实时通信场景

## 示例程序说明

项目包含四个示例程序，展示共享内存队列的不同使用场景：

1. **基础生产者-消费者示例**
   - `producer.cpp`: 生产者程序，创建队列并发送消息
   - `consumer.cpp`: 消费者程序，连接到队列并接收消息

2. **双向通信示例**
   - `bidirectional_demo.cpp`: 演示如何使用两个队列建立双向通信通道，支持命令-响应模式

3. **性能测试**
   - `perf_test.cpp`: 测试队列在不同负载下的性能表现

## 编译

### 环境要求

- CMake 3.10+
- C++17兼容的编译器
- Linux系统 (依赖共享内存API)

### 构建步骤

```bash
# 创建并进入构建目录
mkdir -p build
cd build

# 配置和编译
cmake ..
make

# 也可以只编译特定目标
make producer consumer
```

## 运行示例

### 基础生产者-消费者示例

在两个不同的终端窗口中运行：

**终端1（生产者）：**
```bash
./producer --name /my_queue --size 100 --interval 500
```

**终端2（消费者）：**
```bash
./consumer --name /my_queue --size 100
```

命令行选项：
- `--name`: 共享内存名称 (必须以'/'开头)
- `--size`: 队列大小 (元素数量)
- `--interval`: 生产消息的间隔(毫秒)
- `--latest`: 消费者选项，只获取最新消息，忽略旧消息

### 双向通信示例

在一个终端中运行：
```bash
./bidirectional_demo
```

该程序使用多线程模拟两个进程间的双向通信，展示命令-响应模式。

### 性能测试

```bash
./perf_test --queue-size 1024 --messages 1000000
```

命令行选项：
- `--queue-size`: 队列大小
- `--messages`: 测试消息数量
- `--small-only/--medium-only/--large-only`: 选择测试数据大小

## 注意事项

1. 共享内存名称必须以'/'开头
2. 确保生产者和消费者使用相同的队列大小
3. 共享内存对象在系统重启前会一直存在，除非手动删除：
   ```
   sudo rm /dev/shm/你的共享内存名称
   ```
4. 为避免内存泄漏，确保程序正常退出以清理资源

## 性能优化

- 使用2的幂作为队列大小可获得更好性能
- 对于大数据传输，考虑传递引用或指针而非整个对象
- 调整批处理大小可以优化吞吐量和延迟之间的平衡

## 源代码参考

关键头文件:
- `bounded_spsc_lockfree_queue.h`: 基本无锁队列实现
- `shm_bounded_spsc_lockfree_queue.h`: 共享内存扩展版本
