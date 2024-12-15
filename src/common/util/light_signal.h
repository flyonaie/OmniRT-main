// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.
//
// 轻量级信号量工具
// 提供基于条件变量的线程同步机制，适用于简单的线程间通知场景。
//
// 主要特性:
// 1. 轻量级设计
//    - 基于std::condition_variable实现
//    - 内存占用小，性能开销低
// 2. 易用性
//    - 简单的API设计
//    - 自动的锁管理
// 3. 功能完备
//    - 支持无限等待
//    - 支持超时等待
//    - 支持状态重置
//
// 使用场景:
// - 线程间的事件通知
// - 简单的生产者-消费者模型
// - 异步任务完成通知
//
// 性能考虑:
// - 适用于低频率的线程同步
// - 不适合高并发场景
// - 每次操作都需要加锁

#pragma once

#include <condition_variable>
#include <mutex>

namespace aimrt::common::util {

/**
 * @brief 轻量级信号量类
 * 
 * 提供了一个基于条件变量的简单线程同步机制。
 * 可以用于线程间的事件通知，支持等待和通知操作。
 * 
 * 示例用法:
 * @code
 *   LightSignal signal;
 *   
 *   // 线程1: 等待信号
 *   signal.Wait();
 *   
 *   // 线程2: 发送信号
 *   signal.Notify();
 *   
 *   // 带超时的等待
 *   if (signal.WaitFor(std::chrono::seconds(1))) {
 *     // 成功接收到信号
 *   } else {
 *     // 等待超时
 *   }
 * @endcode
 */
class LightSignal {
 public:
  LightSignal() = default;
  ~LightSignal() = default;

  /**
   * @brief 发送信号
   * 
   * 将信号状态设置为已触发，并唤醒所有等待的线程。
   * 如果当前没有线程在等待，信号状态会被保持。
   * 
   * 线程安全: 是
   * 异常安全: 不抛出异常
   */
  void Notify() {
    std::lock_guard<std::mutex> lck(mutex_);
    flag_ = true;
    cond_.notify_all();
  }

  /**
   * @brief 等待信号
   * 
   * 如果信号未被触发，则阻塞当前线程直到:
   * 1. 信号被触发
   * 2. 线程被虚假唤醒(此时会自动重新检查条件)
   * 
   * 线程安全: 是
   * 异常安全: 不抛出异常
   */
  void Wait() {
    std::unique_lock<std::mutex> lck(mutex_);
    if (flag_) return;
    cond_.wait(lck);
  }

  /**
   * @brief 带超时的等待信号
   * 
   * @param timeout 最大等待时间
   * @return true 成功接收到信号
   * @return false 等待超时
   * 
   * 如果信号未被触发，则阻塞当前线程直到:
   * 1. 信号被触发
   * 2. 超过指定的等待时间
   * 3. 线程被虚假唤醒(此时会自动重新检查条件)
   * 
   * 线程安全: 是
   * 异常安全: 不抛出异常
   */
  bool WaitFor(std::chrono::nanoseconds timeout) {
    std::unique_lock<std::mutex> lck(mutex_);
    if (flag_) return true;
    if (cond_.wait_for(lck, timeout) == std::cv_status::timeout) return false;
    return true;
  }

  /**
   * @brief 重置信号状态
   * 
   * 将信号状态设置为未触发。
   * 通常在处理完一次信号后调用，为下一次等待做准备。
   * 
   * 线程安全: 是
   * 异常安全: 不抛出异常
   */
  void Reset() {
    std::lock_guard<std::mutex> lck(mutex_);
    flag_ = false;
  }

 private:
  std::mutex mutex_;              ///< 互斥锁，保护flag_的访问
  std::condition_variable cond_;  ///< 条件变量，用于线程等待和通知
  bool flag_ = false;             ///< 信号状态标志
};

}  // namespace aimrt::common::util