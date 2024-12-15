// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.
//
// Futex原子操作工具
// 提供基于Linux futex系统调用的轻量级同步原语，结合自旋锁和futex实现高效的线程同步。
//
// 主要特性:
// 1. 两阶段等待策略
//    - 第一阶段: 自旋等待，适用于短期锁定
//    - 第二阶段: futex等待，适用于长期锁定
// 2. 内存效率
//    - 仅使用4字节对齐的原子变量
//    - 无额外内存分配
// 3. 性能优化
//    - 使用CPU PAUSE指令优化自旋等待
//    - 避免不必要的系统调用
// 4. 灵活性
//    - 支持单个/全部线程唤醒
//    - 支持自定义内存序
//
// 使用场景:
// - 线程间的高效同步
// - 条件变量的替代方案
// - 自旋锁的替代方案
//
// 性能考虑:
// - 短期等待时自旋效率更高
// - 长期等待时切换到futex避免CPU浪费
// - 内存对齐优化缓存访问

#pragma once

#include <atomic>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace aimrt::common::util {

/**
 * @brief Linux futex系统调用的封装
 * 
 * @param uaddr 等待地址，必须是4字节对齐
 * @param op 操作类型，如FUTEX_WAIT_PRIVATE, FUTEX_WAKE_PRIVATE等
 * @param val 与操作相关的值
 * @param timeout 超时时间，nullptr表示永久等待
 * @param uaddr2 用于某些操作的第二个地址
 * @param val3 用于某些操作的第二个值
 * @return 成功返回0，失败返回-1
 * 
 * 常见操作类型:
 * - FUTEX_WAIT_PRIVATE: 如果*uaddr == val，则等待唤醒
 * - FUTEX_WAKE_PRIVATE: 唤醒最多val个等待线程
 */
inline long futex(uint32_t* uaddr, int op, uint32_t val,
                 const struct timespec* timeout = nullptr,
                 uint32_t* uaddr2 = nullptr, uint32_t val3 = 0) {
  return syscall(SYS_futex, uaddr, op, val, timeout, uaddr2, val3);
}

/**
 * @brief 基于futex的原子布尔类型
 * 
 * 提供比std::atomic<bool>更高效的线程同步机制：
 * 1. 短时间的等待使用自旋锁
 * 2. 长时间的等待切换到futex系统调用
 * 
 * 示例用法:
 * @code
 *   FutexAtomic flag;
 *   
 *   // 线程1
 *   flag.store(true);  // 设置标志
 *   flag.notify_one(); // 唤醒一个等待线程
 *   
 *   // 线程2
 *   flag.wait(false);  // 等待标志变为true
 * @endcode
 */
class FutexAtomic {
 public:
  FutexAtomic() noexcept = default;
  ~FutexAtomic() = default;

  /**
   * @brief 等待原子值变为非old值
   * 
   * 实现两阶段等待策略:
   * 1. 首先进行自旋等待(SPIN_COUNT次)
   * 2. 如果仍未满足条件，切换到futex等待
   * 
   * @param old 期望的旧值
   */
  void wait(bool old) const noexcept {
    // 首先进行快速检查和自旋
    static constexpr int SPIN_COUNT = 4000;
    for (int i = 0; i < SPIN_COUNT; ++i) {
      if (flag_.load(std::memory_order_acquire) != old) {
        return;
      }
      asm volatile("pause" ::: "memory");  // Intel PAUSE 指令
    }

    // 如果自旋后仍未满足条件，使用 futex
    while (flag_.load(std::memory_order_acquire) == old) {
      futex(reinterpret_cast<uint32_t*>(const_cast<std::atomic<bool>*>(&flag_)),
            FUTEX_WAIT_PRIVATE, old ? 1 : 0);
    }
  }

  /**
   * @brief 唤醒一个等待的线程
   * 
   * 使用FUTEX_WAKE_PRIVATE操作唤醒一个等待线程
   * 如果没有等待的线程，这个操作不会产生任何效果
   */
  void notify_one() noexcept {
    futex(reinterpret_cast<uint32_t*>(&flag_), FUTEX_WAKE_PRIVATE, 1);
  }

  /**
   * @brief 唤醒所有等待的线程
   * 
   * 使用FUTEX_WAKE_PRIVATE操作唤醒所有等待线程
   * 通过传递INT_MAX作为最大唤醒数量来实现
   */
  void notify_all() noexcept {
    futex(reinterpret_cast<uint32_t*>(&flag_), FUTEX_WAKE_PRIVATE, INT_MAX);
  }

  /**
   * @brief 存储新值
   * 
   * @param desired 期望存储的新值
   * @param order 内存序，默认为顺序一致性
   */
  void store(bool desired, std::memory_order order = std::memory_order_seq_cst) noexcept {
    flag_.store(desired, order);
  }

  /**
   * @brief 加载当前值
   * 
   * @param order 内存序，默认为顺序一致性
   * @return 当前存储的布尔值
   */
  bool load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
    return flag_.load(order);
  }

  /**
   * @brief 布尔转换操作符
   * @return 当前存储的布尔值
   */
  operator bool() const noexcept {
    return load();
  }

  /**
   * @brief 赋值操作符
   * 
   * @param desired 要存储的新值
   * @return 当前对象的引用
   */
  FutexAtomic& operator=(bool desired) noexcept {
    store(desired);
    return *this;
  }

 private:
  // 使用4字节对齐确保原子操作的正确性
  alignas(4) std::atomic<bool> flag_{false};
};

#if __cplusplus >= 202002L
#include <version>
#endif

}  // namespace aimrt::common::util
