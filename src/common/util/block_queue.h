// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>

namespace aimrt::common::util {

/**
 * @brief 阻塞队列停止异常类
 * 
 * 当阻塞队列被停止后，继续进行入队或出队操作时抛出该异常
 */
class BlockQueueStoppedException : public std::runtime_error {
 public:
  BlockQueueStoppedException() : std::runtime_error("BlockQueue is stopped") {}
};

/**
 * @brief 线程安全的阻塞队列实现
 * 
 * @tparam T 队列中存储的元素类型
 * 
 * 该队列具有以下特点：
 * 1. 支持多线程安全的入队和出队操作
 * 2. 提供阻塞和非阻塞的出队方式
 * 3. 支持优雅停止队列操作
 * 4. 支持左值和右值入队
 */
template <class T>
class BlockQueue {
 public:
  BlockQueue() = default;
  ~BlockQueue() { Stop(); }

  // 禁用拷贝构造和赋值操作，保证队列的唯一性
  BlockQueue(const BlockQueue &) = delete;
  BlockQueue &operator=(const BlockQueue &) = delete;

  /**
   * @brief 将元素入队（左值版本）
   * 
   * @param item 待入队的元素
   * @throw BlockQueueStoppedException 如果队列已停止则抛出异常
   */
  void Enqueue(const T &item) {
    std::unique_lock<std::mutex> lck(mutex_);
    if (!running_flag_) throw BlockQueueStoppedException();
    queue_.emplace(item);
    cond_.notify_one();
  }

  /**
   * @brief 将元素入队（右值版本）
   * 
   * @param item 待入队的元素
   * @throw BlockQueueStoppedException 如果队列已停止则抛出异常
   */
  void Enqueue(T &&item) {
    std::unique_lock<std::mutex> lck(mutex_);
    if (!running_flag_) throw BlockQueueStoppedException();
    queue_.emplace(std::move(item));
    cond_.notify_one();
  }

  /**
   * @brief 阻塞式出队操作
   * 
   * @return T 队首元素
   * @throw BlockQueueStoppedException 如果队列已停止则抛出异常
   * 
   * 当队列为空时，调用线程将被阻塞，直到：
   * 1. 有新元素入队
   * 2. 队列被停止
   */
  T Dequeue() {
    std::unique_lock<std::mutex> lck(mutex_);
    cond_.wait(lck, [this] { return !queue_.empty() || !running_flag_; });
    if (!running_flag_) throw BlockQueueStoppedException();
    T item = std::move(queue_.front());
    queue_.pop();
    return item;
  }

  /**
   * @brief 非阻塞式出队操作
   * 
   * @return std::optional<T> 如果队列非空，返回队首元素；否则返回std::nullopt
   * 
   * 该方法不会阻塞调用线程，适用于无需等待的场景
   */
  std::optional<T> TryDequeue() {
    std::lock_guard<std::mutex> lck(mutex_);
    if (queue_.empty() || !running_flag_) [[unlikely]]
      return std::nullopt;

    T item = std::move(queue_.front());
    queue_.pop();
    return item;
  }

  /**
   * @brief 停止队列操作
   * 
   * 停止后的效果：
   * 1. 所有阻塞的线程被唤醒
   * 2. 后续的入队和出队操作将抛出异常
   */
  void Stop() {
    std::unique_lock<std::mutex> lck(mutex_);
    running_flag_ = false;
    cond_.notify_all();
  }

  /**
   * @brief 获取队列当前大小
   * 
   * @return size_t 队列中的元素个数
   */
  size_t Size() const {
    std::lock_guard<std::mutex> lck(mutex_);
    return queue_.size();
  }

  /**
   * @brief 检查队列是否在运行状态
   * 
   * @return bool 如果队列正在运行返回true，否则返回false
   */
  bool IsRunning() const {
    std::lock_guard<std::mutex> lck(mutex_);
    return running_flag_;
  }

 protected:
  mutable std::mutex mutex_;          ///< 互斥锁，保护队列的并发访问
  std::condition_variable cond_;      ///< 条件变量，用于线程同步
  std::queue<T> queue_;              ///< 底层队列容器
  bool running_flag_ = true;         ///< 队列运行状态标志
};
}  // namespace aimrt::common::util
