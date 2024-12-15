// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.
//
// 延迟执行工具类
// 提供了一个类似Go语言defer语句的机制,用于确保在作用域结束时
// 执行清理或其他收尾工作。这个实现基于RAII原理,利用对象析构
// 时自动执行预定的操作。
//
// 主要特性:
// 1. 支持任意可调用对象(函数、lambda等)
// 2. 支持移动语义
// 3. 提供显式取消执行的机制
// 4. 保证异常安全
//
// 典型用途:
// - 资源清理(文件句柄、锁等)
// - 状态恢复
// - 计时和性能统计
// - 日志记录
//
// 示例:
//   {
//     auto lock = mutex.lock();
//     Deferred unlock([&]{ mutex.unlock(); });
//     // 临界区代码...
//   } // 自动解锁

#pragma once
#include <functional>
#include <type_traits>

namespace aimrt::common::util {

/**
 * @brief 延迟执行类
 * 
 * 在对象析构时执行预定的操作。这个实现借鉴自trpc-cpp项目,
 * 提供了类似Go语言defer语句的功能。
 * 
 * 特点:
 * - 基于RAII设计模式
 * - 支持移动语义
 * - 提供显式取消机制
 * - 保证异常安全
 */
class Deferred {
 public:
  /// @brief 默认构造函数,创建一个空的Deferred对象
  Deferred() = default;

  /**
   * @brief 构造函数,接受一个可调用对象
   * 
   * @tparam F 可调用对象类型
   * @param f 要延迟执行的操作
   */
  template <class F,
            typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Deferred>>>
  explicit Deferred(F&& f) : action_(std::forward<F>(f)) {}

  /// @brief 移动构造函数
  Deferred(Deferred&&) noexcept = default;
  /// @brief 移动赋值运算符
  Deferred& operator=(Deferred&&) noexcept = default;

  /**
   * @brief 析构函数,执行预定的操作
   * 
   * 如果存在有效的action_且未被取消,则执行它
   */
  ~Deferred() {
    if (action_) {
      action_();
    }
  }

  /**
   * @brief 检查是否有待执行的操作
   * 
   * @return true 有待执行的操作
   * @return false 没有待执行的操作
   */
  explicit operator bool() const noexcept { return !!action_; }

  /**
   * @brief 取消预定的操作
   * 
   * 调用此方法后,析构时将不会执行预定的操作
   */
  void Dismiss() noexcept { action_ = nullptr; }

 private:
  std::function<void()> action_;  ///< 存储待执行的操作
};

}  // namespace aimrt::common::util
