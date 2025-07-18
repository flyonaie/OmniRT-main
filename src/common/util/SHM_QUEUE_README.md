# 基于共享内存的有界单生产者单消费者无锁队列

## 简介

`ShmBoundedSpscLockfreeQueue` 是一个基于共享内存的有界单生产者单消费者(SPSC)无锁队列实现，专为高性能进程间通信场景设计。
此队列支持在不同进程间安全高效地传递数据，同时保持极低的延迟和高吞吐量。

## 主要特性

- **基于共享内存**：支持跨进程数据传输，可用于分布式应用或多进程架构
- **无锁设计**：使用原子操作代替互斥锁，避免上下文切换开销
- **有界队列**：预分配固定大小的存储空间，避免动态内存分配
- **SPSC特化**：专为单生产者-单消费者场景优化，提供最佳性能
- **缓存友好**：通过内存对齐减少伪共享，提升缓存命中率
- **支持复杂数据类型**：可用于传输自定义结构体和类（需确保在共享内存中安全）

## 使用场景

- 实时系统中的进程间数据传输
- 多进程架构中的高性能消息传递
- 分布式应用中的本地节点间通信
- 对延迟敏感的应用程序

## 使用方法

### 基本用法

```cpp
#include "util/shm_bounded_spsc_lockfree_queue.h"

// 创建者进程
omnirt::common::util::ShmBoundedSpscLockfreeQueue<int> queue;
if (queue.Init("/my_queue", 1024, false, true)) {
    // 入队操作
    queue.Enqueue(42);
}

// 附加者进程
omnirt::common::util::ShmBoundedSpscLockfreeQueue<int> queue;
if (queue.Init("/my_queue", 1024, false, false)) {
    // 出队操作
    int value;
    if (queue.Dequeue(&value)) {
        std::cout << "接收到数据: " << value << std::endl;
    }
}
```

### 使用自定义数据类型

```cpp
struct MyData {
    int id;
    double value;
    char message[64];
};

omnirt::common::util::ShmBoundedSpscLockfreeQueue<MyData> queue;
if (queue.Init("/data_queue", 1024)) {
    MyData data = {1, 3.14, "Hello, Shared Memory!"};
    queue.Enqueue(data);
}
```

### 高级功能

- **覆盖式入队**：当队列已满时覆盖最老的数据
  ```cpp
  queue.EnqueueOverwrite(data);  // 总是成功入队，必要时覆盖旧数据
  ```

- **获取最新数据**：忽略旧数据，只获取最新数据
  ```cpp
  MyData latest_data;
  queue.DequeueLatest(&latest_data);  // 跳过所有旧数据，只获取最新数据
  ```

- **关闭和清理**：正确释放共享内存资源
  ```cpp
  queue.Close();  // 关闭共享内存连接
  shm_unlink("/my_queue");  // 删除共享内存对象（通常由创建者执行）
  ```

## 注意事项

1. **数据类型要求**：
   - 最好使用POD类型(Plain Old Data)
   - 不要在共享内存中存储指针、引用或包含STL容器的类型
   - 复杂类型必须确保在共享内存中正确构造和析构

2. **进程生命周期**：
   - 创建者应负责创建和删除共享内存对象
   - 附加者只应附加到现有共享内存
   - 最后退出的进程应该执行`shm_unlink()`删除共享内存对象

3. **性能优化**：
   - 使用2的幂次容量可获得更好的索引计算性能
   - 控制队列大小以平衡内存使用和性能

4. **跨机器限制**：
   - 共享内存只能在同一物理机器的进程间使用，不支持跨网络通信

## 示例程序

参见 `shm_bounded_spsc_lockfree_queue_example.cc` 获取完整的使用示例，包括：
- 单进程中的生产者-消费者线程
- 多进程通信模式

## 性能考虑

- 无锁设计使得入队/出队操作的延迟极低（通常在几百纳秒级别）
- 共享内存访问比网络通信快几个数量级
- 使用`EnqueueOverwrite`和`DequeueLatest`可以实现无阻塞的实时数据传输
