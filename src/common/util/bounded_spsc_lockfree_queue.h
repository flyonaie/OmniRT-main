#ifndef OMNIRT_COMMON_UTIL_BOUNDED_SPSC_LOCKFREE_QUEUE_H_
#define OMNIRT_COMMON_UTIL_BOUNDED_SPSC_LOCKFREE_QUEUE_H_

#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <utility>

#ifndef CACHELINE_SIZE
#define CACHELINE_SIZE 64
#endif

namespace omnirt::common::util {

template <typename T>
class BoundedSpscLockfreeQueue {
 public:
  // 构造和析构
  BoundedSpscLockfreeQueue() = default;
  ~BoundedSpscLockfreeQueue();
  
  // 禁用拷贝
  BoundedSpscLockfreeQueue(const BoundedSpscLockfreeQueue& other) = delete;
  BoundedSpscLockfreeQueue& operator=(const BoundedSpscLockfreeQueue& other) = delete;

  // 初始化方法
  bool Init(uint64_t size);

  // 基本操作
  bool Enqueue(const T& element);
  bool Enqueue(T&& element);
  bool Dequeue(T* element);
  
  // 状态查询
  uint64_t Size();
  bool Empty();

  // 元素访问
  bool TailElement(T* element) { 
    if (!element || Empty()) {
      return false;
    }
    const uint64_t cur_tail = tail_.load(std::memory_order_acquire);
    *element = pool_[GetIndex(cur_tail - 1)];  // 获取最后一个元素，cur_tail指向并未存放数据
    return true;
  }
  
  bool GetElement(uint64_t index, T* element) { 
    if (!element) {
      return false;
    }
    *element = pool_[GetIndex(index)];
    return true; 
  }

  // 位置查询
  uint64_t Head() { return head_.load(std::memory_order_acquire); }
  uint64_t Tail() { return tail_.load(std::memory_order_acquire); }

 private:
  // 私有方法
  uint64_t GetIndex(uint64_t num) {
    return num & pool_size_mask_;  // 使用预计算的掩码，更快
  }

  // 数据成员
  alignas(CACHELINE_SIZE) std::atomic<uint64_t> head_{0};
  alignas(CACHELINE_SIZE) std::atomic<uint64_t> tail_{0};
  const uint64_t QUEUE_MAX_SIZE = (1ULL << 63) - 1;  // 修正2^64-1的写法
  uint64_t pool_size_{0};
  uint64_t pool_size_mask_{0};  // 添加掩码缓存，避免重复计算
  T* pool_{nullptr};
};

template <typename T>
BoundedSpscLockfreeQueue<T>::~BoundedSpscLockfreeQueue() {
  if (pool_) {
    for (uint64_t i = 0; i < pool_size_; ++i) {
      pool_[i].~T();
    }
    std::free(pool_);
  }
}

template <typename T>
bool BoundedSpscLockfreeQueue<T>::Init(uint64_t size) {
  if (pool_ != nullptr) {
    return false;
  }

  if (size > QUEUE_MAX_SIZE) {
    return false;
  }

  // Round up to the next power of 2
  pool_size_ = 1;
  while (pool_size_ < size) {
    pool_size_ <<= 1;
  }
  
  pool_size_mask_ = pool_size_ - 1;  // 预计算掩码
  if ((pool_size_ & pool_size_mask_) != 0) {  // 确保是2的幂
    return false;
  }

  pool_ = reinterpret_cast<T*>(std::aligned_alloc(CACHELINE_SIZE, pool_size_ * sizeof(T)));  // 使用对齐内存分配
  if (pool_ == nullptr) {
    return false;
  }
  
  // 使用placement new初始化对象
  for (uint64_t i = 0; i < pool_size_; ++i) {
    new (&pool_[i]) T();
  }

  return true;
}

template <typename T>
bool BoundedSpscLockfreeQueue<T>::Enqueue(const T& element) {
  const uint64_t cur_tail = tail_.load(std::memory_order_relaxed);
  const uint64_t cur_head = head_.load(std::memory_order_acquire);
  
  // 修正队列满的判断条件
  if (cur_tail - cur_head >= pool_size_) {
    return false; // 队列满
  }

  // 先写入数据，再更新tail
  pool_[GetIndex(cur_tail)] = element;
  std::atomic_thread_fence(std::memory_order_release);
  tail_.store(cur_tail + 1, std::memory_order_release);

  return true;
}

template <typename T>
bool BoundedSpscLockfreeQueue<T>::Enqueue(T&& element) {
  const uint64_t cur_tail = tail_.load(std::memory_order_relaxed);
  const uint64_t cur_head = head_.load(std::memory_order_acquire);
  
  // 修正队列满的判断条件
  if (cur_tail - cur_head >= pool_size_) {
    return false; // 队列满
  }

  // 先写入数据，再更新tail
  pool_[GetIndex(cur_tail)] = std::move(element);
  std::atomic_thread_fence(std::memory_order_release);
  tail_.store(cur_tail + 1, std::memory_order_release);

  return true;
}

template <typename T>
bool BoundedSpscLockfreeQueue<T>::Dequeue(T* element) {
  if (!element) {
    return false;
  }
  
  const uint64_t cur_head = head_.load(std::memory_order_relaxed);
  const uint64_t cur_tail = tail_.load(std::memory_order_acquire);
  
  if (cur_head == cur_tail) {
    return false;  // 队列空
  }

  // 先读取数据，再更新head
  *element = std::move(pool_[GetIndex(cur_head)]);
  std::atomic_thread_fence(std::memory_order_release);
  head_.store(cur_head + 1, std::memory_order_release);

  return true;
}

template <typename T>
uint64_t BoundedSpscLockfreeQueue<T>::Size() {
  const uint64_t cur_head = head_.load(std::memory_order_acquire);
  const uint64_t cur_tail = tail_.load(std::memory_order_acquire);
  const uint64_t size = cur_tail - cur_head;
  return size <= pool_size_ ? size : pool_size_;
}

template <typename T>
bool BoundedSpscLockfreeQueue<T>::Empty() {
  const uint64_t cur_head = head_.load(std::memory_order_acquire);
  const uint64_t cur_tail = tail_.load(std::memory_order_acquire);
  return cur_head == cur_tail;
}

}  // namespace omnirt::common::util

#endif  // OMNIRT_COMMON_UTIL_BOUNDED_SPSC_LOCKFREE_QUEUE_H_
