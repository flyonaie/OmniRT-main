// Copyright (c) 2023 OmniRT Authors. All rights reserved.
//
// 有界单生产者单消费者无锁队列实现
// 本队列实现专门针对单生产者-单消费者场景进行了优化,具有以下特点:
// 1. 无锁实现: 通过原子操作和内存序保证线程安全,避免锁开销
// 2. 有界队列: 预分配固定大小的存储空间,避免动态内存分配
// 3. SPSC特化: 仅支持单生产者-单消费者模式,但性能最优
// 4. 缓存友好: 通过字节对齐减少伪共享,提升缓存命中率
//
// 使用场景:
// - 高性能线程间通信,如生产者-消费者模型
// - 实时系统中的数据传输
// - 对延迟敏感的应用程序
//
// 线程安全说明:
// - 一个线程可以安全地执行入队操作(生产者)
// - 一个线程可以安全地执行出队操作(消费者) 
// - 同一个线程不能同时执行入队和出队操作

#pragma once

#include <atomic>
#include <cstddef>  // for size_t
#include <cstdint>
#include <cstdlib>  // for free
#include <utility>
#include <new>
#include <cassert>
#include <cstring>

// C++11 兼容的对齐内存分配函数
#if !defined(_MSC_VER)
inline void* aligned_malloc(size_t alignment, size_t size) {
    void* ptr = nullptr;
    int ret = posix_memalign(&ptr, alignment, size);
    return ret == 0 ? ptr : nullptr;
}

inline void aligned_free(void* ptr) {
    free(ptr);
}
#else
inline void* aligned_malloc(size_t alignment, size_t size) {
    return _aligned_malloc(size, alignment);
}

inline void aligned_free(void* ptr) {
    _aligned_free(ptr);
}
#endif

namespace omnirt::common::util {

/**
 * @brief 有界单生产者单消费者无锁队列
 * 
 * 这是一个高性能的线程间通信队列,专门为单生产者-单消费者场景优化。
 * 队列使用无锁设计,通过原子操作和内存序(memory ordering)保证线程安全。
 * 
 * 主要特性:
 * 1. 预分配固定大小的存储空间,避免运行时内存分配
 * 2. 使用原子操作代替互斥锁,降低同步开销
 * 3. 通过内存对齐优化缓存访问,减少伪共享
 * 4. 支持普通入队/出队和覆盖式入队/最新出队等操作
 * 
 * 注意事项:
 * - 队列大小在初始化时指定,之后不可更改
 * - 当指定2的幂大小时,索引计算会使用位运算优化
 * - 生产者和消费者必须是不同的线程
 * 
 * @tparam T 队列中存储的元素类型
 */
template <typename T>
class BoundedSpscLockfreeQueue {
 public:
  // 常量定义
  static constexpr uint64_t QUEUE_MAX_SIZE = (1ULL << 63) - 1;
  static constexpr size_t CACHELINE_SIZE = 64;

  // 构造和析构
  BoundedSpscLockfreeQueue() = default;
  ~BoundedSpscLockfreeQueue();
  
  // 禁用拷贝和移动
  BoundedSpscLockfreeQueue(const BoundedSpscLockfreeQueue&) = delete;
  BoundedSpscLockfreeQueue& operator=(const BoundedSpscLockfreeQueue&) = delete;
  BoundedSpscLockfreeQueue(BoundedSpscLockfreeQueue&&) = delete;
  BoundedSpscLockfreeQueue& operator=(BoundedSpscLockfreeQueue&&) = delete;

  /**
   * @brief 初始化队列
   * 
   * 为队列分配指定大小的存储空间并进行初始化。如果指定force_power_of_two为true,
   * 则size必须是2的幂,这样可以使用位运算来优化索引计算。
   * 
   * @param size 队列大小,必须大于0且不超过QUEUE_MAX_SIZE
   * @param force_power_of_two 是否强制size为2的幂
   * @return true 初始化成功
   * @return false 初始化失败(size无效或内存分配失败)
   */
  bool Init(uint64_t size, bool force_power_of_two = false);

  /**
   * @brief 将元素入队
   * 
   * 尝试将元素添加到队列尾部。如果队列已满,则入队失败。
   * 该方法线程安全,但只能由生产者线程调用。
   * 
   * @param element 要入队的元素
   * @return true 入队成功
   * @return false 队列已满或未初始化
   */
  bool Enqueue(const T& element) { return EnqueueInternal(element, false); }
  bool Enqueue(T&& element) { return EnqueueInternal(std::move(element), false); }
  
  /**
   * @brief 强制将元素入队
   * 
   * 将元素添加到队列尾部。如果队列已满,则覆盖最旧的元素。
   * 该方法线程安全,但只能由生产者线程调用。
   * 
   * @param element 要入队的元素
   * @return true 入队成功(除非队列未初始化,否则总是成功)
   * @return false 队列未初始化
   */
  bool EnqueueOverwrite(const T& element) { return EnqueueInternal(element, true); }
  bool EnqueueOverwrite(T&& element) { return EnqueueInternal(std::move(element), true); }

  /**
   * @brief 从队列中取出最早的元素
   * 
   * 尝试从队列头部取出元素。如果队列为空,则出队失败。
   * 该方法线程安全,但只能由消费者线程调用。
   * 
   * @param[out] element 用于存储出队元素的指针
   * @return true 出队成功
   * @return false 队列为空或未初始化或element为空
   */
  bool Dequeue(T* element);

  /**
   * @brief 从队列中取出最新的元素
   * 
   * 尝试获取队列中最新的元素(tail-1位置),并丢弃所有更早的元素。
   * 该方法线程安全,但只能由消费者线程调用。
   * 
   * @param[out] element 用于存储出队元素的指针
   * @return true 出队成功
   * @return false 队列为空或未初始化或element为空
   */
  bool DequeueLatest(T* element);

  // 状态查询
  /**
   * @brief 返回队列当前元素数量
   * 
   * 获取队列中当前的元素数量。
   * 
   * @return 当前队列大小
   */
  uint64_t Size() const;
  
  /**
   * @brief 检查队列是否为空
   * 
   * 判断队列是否为空。
   * 
   * @return true 队列为空
   * @return false 队列不为空
   */
  bool Empty() const { return Size() == 0; }
  
  /**
   * @brief 返回队列最大容量
   * 
   * 获取队列的最大容量。
   * 
   * @return 队列容量
   */
  uint64_t Capacity() const { return pool_size_; }

 protected:
  // 存储在共享内存中的队列头部结构
  struct alignas(CACHELINE_SIZE) QueueHeader {
    std::atomic<uint64_t> head_{0};  // 消费者使用
    char padding1[CACHELINE_SIZE - sizeof(std::atomic<uint64_t>)];
    std::atomic<uint64_t> tail_{0};  // 生产者使用
    char padding2[CACHELINE_SIZE - sizeof(std::atomic<uint64_t>)];
    uint64_t pool_size_{0};        // 队列容量
    uint64_t pool_size_mask_{0};   // 用于优化2的幂大小时的索引计算
    bool use_mask_{false};         // 是否使用掩码计算(取决于size是否为2的幂)
  };

  // 成员变量
  QueueHeader* header_{nullptr};  // 指向共享内存中的队列头部结构
  uint64_t pool_size_{0};            // 队列容量(本地缓存)
  T* pool_{nullptr};             // 元素存储区域 

 protected:
  // 受保护的辅助方法
  bool EnqueueInternal(const T& element, bool overwrite);
  
 private:
  
  /**
   * @brief 检查一个数是否是2的幂
   * 
   * 判断一个数是否是2的幂。
   * 
   * @param x 要检查的数
   * @return true x是2的幂
   * @return false x不是2的幂
   */
  static bool IsPowerOfTwo(uint64_t x) { return x > 0 && (x & (x - 1)) == 0; }
  
  /**
   * @brief 计算循环缓冲区中的索引
   * 
   * 根据逻辑位置计算实际的缓冲区索引。
   * 
   * @param num 逻辑位置
   * @return 实际缓冲区索引
   */
  uint64_t GetIndex(uint64_t num) const {
    // 2的幂情况：位掩码(~0.5ns),非2的幂情况：使用减法方法(~12ns)
    return header_->use_mask_ ? (num & header_->pool_size_mask_)
							 : (num - (num / header_->pool_size_) * header_->pool_size_);
  }


  // QueueHeader结构体已移至protected部分

  // 成员变量已移至protected部分 
};

template <typename T>
BoundedSpscLockfreeQueue<T>::~BoundedSpscLockfreeQueue() {
  if (header_ && pool_) {
    // 析构所有元素并释放内存
    for (uint64_t i = 0; i < header_->pool_size_; ++i) {
      pool_[i].~T();
    }
    aligned_free(pool_);
  }
  
  // 释放队列头部结构内存
  if (header_) {
    header_->~QueueHeader();
    aligned_free(header_);
  }
}

template <typename T>
bool BoundedSpscLockfreeQueue<T>::Init(uint64_t size, bool force_power_of_two) {
  if (header_ != nullptr || pool_ != nullptr || size == 0 || size > QUEUE_MAX_SIZE) {
    return false;
  }

  if (force_power_of_two && !IsPowerOfTwo(size)) {
    // 如果要求2的幂但size不是2的幂，直接返回错误
    return false;
  }

  // 动态分配并初始化队列头部结构
  // C++11兼容的分配方式，替换std::aligned_alloc
  header_ = static_cast<QueueHeader*>(aligned_malloc(CACHELINE_SIZE, sizeof(QueueHeader)));
  if (header_ == nullptr) {
    return false;
  }
  
  // 初始化对齐到缓存行的队列头部
  new (header_) QueueHeader();
  header_->head_.store(0, std::memory_order_relaxed);
  header_->tail_.store(0, std::memory_order_relaxed);
  header_->pool_size_ = size;
  header_->use_mask_ = force_power_of_two;
  header_->pool_size_mask_ = header_->pool_size_ - 1;

  // C++11兼容的分配方式，替换std::aligned_alloc
  pool_ = static_cast<T*>(aligned_malloc(alignof(T), header_->pool_size_ * sizeof(T)));
  if (pool_ == nullptr) {
    return false;
  }

  // 初始化所有元素
  for (uint64_t i = 0; i < header_->pool_size_; ++i) {
    new (&pool_[i]) T();
  }

  return true;
}

template <typename T>
bool BoundedSpscLockfreeQueue<T>::EnqueueInternal(const T& element, bool overwrite) {
  if (header_ == nullptr || pool_ == nullptr) {
    return false;
  }

  const uint64_t cur_tail = header_->tail_.load(std::memory_order_acquire);
  const uint64_t cur_head = header_->head_.load(std::memory_order_acquire);
  
  // 检查队列是否已满
  if (cur_tail - cur_head >= header_->pool_size_) {
    if (!overwrite) {
      return false;
    }
    // 强制覆盖时，移动head以丢弃最老的元素
    header_->head_.store(cur_tail - header_->pool_size_ + 1, std::memory_order_release);
  }

  // 写入新元素并更新tail
  pool_[GetIndex(cur_tail)] = element;
  header_->tail_.store(cur_tail + 1, std::memory_order_release);  
  return true;
}

template <typename T>
bool BoundedSpscLockfreeQueue<T>::Dequeue(T* element) {
  if (header_ == nullptr || pool_ == nullptr || !element) {
    return false;
  }
  
  // 使用relaxed order加载head，因为只有消费者线程会修改它
  const uint64_t cur_head = header_->head_.load(std::memory_order_relaxed);
  const uint64_t cur_tail = header_->tail_.load(std::memory_order_acquire);
  
  if (cur_head == cur_tail) {
    return false;  // 队列为空
  }

  // 移出元素并更新head
  *element = std::move(pool_[GetIndex(cur_head)]);
  // store操作本身就会确保之前的内存操作不会被重排到store之后
  header_->head_.store(cur_head + 1, std::memory_order_release);
  return true;
}

template <typename T>
bool BoundedSpscLockfreeQueue<T>::DequeueLatest(T* element) {
  if (header_ == nullptr || pool_ == nullptr || !element) {
    return false;
  }
  
  const uint64_t cur_head = header_->head_.load(std::memory_order_relaxed);
  const uint64_t cur_tail = header_->tail_.load(std::memory_order_acquire);
  
  if (cur_head == cur_tail) {
    return false;  // 队列为空
  }

  // 直接获取最新数据（tail-1 位置的数据）
  *element = std::move(pool_[GetIndex(cur_tail - 1)]);
  // store操作本身就会确保之前的内存操作不会被重排到store之后
  header_->head_.store(cur_tail, std::memory_order_release);
  return true;
}

template <typename T>
uint64_t BoundedSpscLockfreeQueue<T>::Size() const {
  if (header_ == nullptr || pool_ == nullptr) {
    return 0;
  }
  const uint64_t cur_head = header_->head_.load(std::memory_order_acquire);
  const uint64_t cur_tail = header_->tail_.load(std::memory_order_acquire);
  // 直接相减，溢出时结果仍然正确
  const uint64_t size = cur_tail - cur_head;
  return size <= header_->pool_size_ ? size : header_->pool_size_;
}

}  // namespace omnirt::common::util
