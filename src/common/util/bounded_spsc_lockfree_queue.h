// Copyright (c) 2023 OmniRT Authors. All rights reserved.
//
// Bounded single-producer single-consumer lock-free queue implementation.
// Thread-safe for one producer thread and one consumer thread.

#ifndef OMNIRT_COMMON_UTIL_BOUNDED_SPSC_LOCKFREE_QUEUE_H_
#define OMNIRT_COMMON_UTIL_BOUNDED_SPSC_LOCKFREE_QUEUE_H_

#include <atomic>
#include <cstddef>  // for size_t
#include <cstdint>
#include <cstdlib>  // for std::free and std::aligned_alloc
#include <utility>

namespace omnirt::common::util {

/**
 * @brief A bounded single-producer single-consumer lock-free queue.
 * 
 * This queue is designed for high-performance communication between exactly
 * one producer thread and one consumer thread. The implementation is lock-free
 * and uses atomic operations for synchronization.
 *
 * Thread Safety:
 *   - One thread can safely enqueue elements (producer)
 *   - One thread can safely dequeue elements (consumer)
 *   - The same thread must not call both enqueue and dequeue
 *
 * @tparam T The type of elements stored in the queue
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
   * @brief Initializes the queue with the specified size
   * @param size The size of the queue (must be > 0 and <= QUEUE_MAX_SIZE)
   * @param force_power_of_two If true, size must be a power of 2 for optimized indexing
   * @return true if initialization succeeds, false otherwise
   */
  bool Init(uint64_t size, bool force_power_of_two = false);

  // 入队操作
  /**
   * @brief Enqueues an element into the queue
   * @param element The element to enqueue
   * @return true if successful, false if queue is full
   */
  bool Enqueue(const T& element) { return EnqueueInternal(element, false); }
  bool Enqueue(T&& element) { return EnqueueInternal(std::move(element), false); }
  
  /**
   * @brief Enqueues an element, overwriting oldest element if queue is full
   * @param element The element to enqueue
   * @return true if successful (always true unless queue is uninitialized)
   */
  bool EnqueueOverwrite(const T& element) { return EnqueueInternal(element, true); }
  bool EnqueueOverwrite(T&& element) { return EnqueueInternal(std::move(element), true); }

  // 出队操作
  /**
   * @brief Dequeues an element from the queue
   * @param[out] element Pointer to store the dequeued element
   * @return true if successful, false if queue is empty
   */
  bool Dequeue(T* element);

  /**
   * @brief Dequeues the latest element, discarding older elements
   * @param[out] element Pointer to store the dequeued element
   * @return true if successful, false if queue is empty
   */
  bool DequeueLatest(T* element);

  // 状态查询
  /**
   * @brief Returns the current number of elements in the queue
   * @return Current queue size
   */
  uint64_t Size() const;
  
  /**
   * @brief Checks if the queue is empty
   * @return true if queue is empty, false otherwise
   */
  bool Empty() const { return Size() == 0; }
  
  /**
   * @brief Returns the maximum capacity of the queue
   * @return Queue capacity
   */
  uint64_t Capacity() const { return pool_size_; }

 private:
  // 私有辅助方法
  bool EnqueueInternal(const T& element, bool overwrite);
  
  /**
   * @brief Checks if a number is a power of two
   * @param x Number to check
   * @return true if x is a power of two
   */
  static bool IsPowerOfTwo(uint64_t x) { return x > 0 && (x & (x - 1)) == 0; }
  
  /**
   * @brief Calculates the index in the circular buffer
   * @param num The logical position
   * @return The actual index in the buffer
   */
  uint64_t GetIndex(uint64_t num) const {
    // 2的幂情况：位掩码（~0.5ns）,非2的幂情况：使用减法方法（~12ns）
    return use_mask_ ? (num & pool_size_mask_) : (num - (num / pool_size_) * pool_size_);
  }

  // 成员变量
  alignas(CACHELINE_SIZE) std::atomic<uint64_t> head_{0};  // 消费者使用
  alignas(CACHELINE_SIZE) std::atomic<uint64_t> tail_{0};  // 生产者使用
  
  uint64_t pool_size_{0};        // 队列容量
  uint64_t pool_size_mask_{0};   // 用于优化2的幂大小时的索引计算
  bool use_mask_{false};         // 是否使用掩码计算（取决于size是否为2的幂）
  T* pool_{nullptr};             // 元素存储区域
};

template <typename T>
BoundedSpscLockfreeQueue<T>::~BoundedSpscLockfreeQueue() {
  if (pool_) {
    // 析构所有元素并释放内存
    for (uint64_t i = 0; i < pool_size_; ++i) {
      pool_[i].~T();
    }
    std::free(pool_);
  }
}

template <typename T>
bool BoundedSpscLockfreeQueue<T>::Init(uint64_t size, bool force_power_of_two) {
  if (pool_ != nullptr || size == 0 || size > QUEUE_MAX_SIZE) {
    return false;
  }

  if (force_power_of_two && !IsPowerOfTwo(size)) {
    // 如果要求2的幂但size不是2的幂，直接返回错误
    return false;
  }

  pool_size_ = size;
  use_mask_ = force_power_of_two;
  pool_size_mask_ = pool_size_ - 1;

  pool_ = static_cast<T*>(std::aligned_alloc(alignof(T), pool_size_ * sizeof(T)));
  if (pool_ == nullptr) {
    return false;
  }

  // 初始化所有元素
  for (uint64_t i = 0; i < pool_size_; ++i) {
    new (&pool_[i]) T();
  }

  return true;
}

template <typename T>
bool BoundedSpscLockfreeQueue<T>::EnqueueInternal(const T& element, bool overwrite) {
  if (pool_ == nullptr) {
    return false;
  }

  const uint64_t cur_tail = tail_.load(std::memory_order_acquire);
  const uint64_t cur_head = head_.load(std::memory_order_acquire);
  
  // 检查队列是否已满
  if (cur_tail - cur_head >= pool_size_) {
    if (!overwrite) {
      return false;
    }
    // 强制覆盖时，移动head以丢弃最老的元素
    head_.store(cur_tail - pool_size_ + 1, std::memory_order_release);
  }

  // 写入新元素并更新tail
  pool_[GetIndex(cur_tail)] = element;
  tail_.store(cur_tail + 1, std::memory_order_release);  
  return true;
}

template <typename T>
bool BoundedSpscLockfreeQueue<T>::Dequeue(T* element) {
  if (pool_ == nullptr || !element) {
    return false;
  }
  
  // 使用relaxed order加载head，因为只有消费者线程会修改它
  const uint64_t cur_head = head_.load(std::memory_order_relaxed);
  const uint64_t cur_tail = tail_.load(std::memory_order_acquire);
  
  if (cur_head == cur_tail) {
    return false;  // 队列为空
  }

  // 移出元素并更新head
  *element = std::move(pool_[GetIndex(cur_head)]);
  // store操作本身就会确保之前的内存操作不会被重排到store之后
  head_.store(cur_head + 1, std::memory_order_release);
  return true;
}

template <typename T>
bool BoundedSpscLockfreeQueue<T>::DequeueLatest(T* element) {
  if (pool_ == nullptr || !element) {
    return false;
  }
  
  const uint64_t cur_head = head_.load(std::memory_order_relaxed);
  const uint64_t cur_tail = tail_.load(std::memory_order_acquire);
  
  if (cur_head == cur_tail) {
    return false;  // 队列为空
  }

  // 直接获取最新数据（tail-1 位置的数据）
  *element = std::move(pool_[GetIndex(cur_tail - 1)]);
  // store操作本身就会确保之前的内存操作不会被重排到store之后
  head_.store(cur_tail, std::memory_order_release);
  return true;
}

template <typename T>
uint64_t BoundedSpscLockfreeQueue<T>::Size() const {
  if (pool_ == nullptr) {
    return 0;
  }
  const uint64_t cur_head = head_.load(std::memory_order_acquire);
  const uint64_t cur_tail = tail_.load(std::memory_order_acquire);
  // 直接相减，溢出时结果仍然正确
  const uint64_t size = cur_tail - cur_head;
  return size <= pool_size_ ? size : pool_size_;
}

}  // namespace omnirt::common::util

#endif  // OMNIRT_COMMON_UTIL_BOUNDED_SPSC_LOCKFREE_QUEUE_H_
