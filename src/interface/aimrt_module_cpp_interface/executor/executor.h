// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <chrono>
#include <string_view>

#include "aimrt_module_c_interface/executor/executor_base.h"
#include "aimrt_module_cpp_interface/util/function.h"
#include "aimrt_module_cpp_interface/util/string.h"
#include "util/exception.h"
#include "util/time_util.h"

namespace aimrt::executor {

/**
 * @brief 任务类型别名,封装了执行器任务操作集
 */
using Task = aimrt::util::Function<aimrt_function_executor_task_ops_t>;

/**
 * @class ExecutorRef
 * @brief 执行器引用类,提供对底层执行器的封装和管理
 * @details 该类提供了对执行器的基本操作接口,包括:
 *          - 任务的同步执行
 *          - 定时任务的调度
 *          - 执行器状态的查询
 */
class ExecutorRef {
 public:
  /** @brief 默认构造函数 */
  ExecutorRef() = default;

  /**
   * @brief 通过底层执行器指针构造
   * @param base_ptr 底层执行器基类指针
   */
  explicit ExecutorRef(const aimrt_executor_base_t* base_ptr)
      : base_ptr_(base_ptr) {}

  /** @brief 析构函数 */
  ~ExecutorRef() = default;

  /**
   * @brief 检查执行器引用是否有效
   * @return 如果引用有效返回true,否则返回false
   */
  explicit operator bool() const { return (base_ptr_ != nullptr); }

  /**
   * @brief 获取底层执行器句柄
   * @return 底层执行器基类指针
   */
  const aimrt_executor_base_t* NativeHandle() const { return base_ptr_; }

  /**
   * @brief 获取执行器类型
   * @return 执行器类型名称
   */
  std::string_view Type() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return aimrt::util::ToStdStringView(base_ptr_->type(base_ptr_->impl));
  }

  /**
   * @brief 获取执行器名称
   * @return 执行器实例名称
   */
  std::string_view Name() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return aimrt::util::ToStdStringView(base_ptr_->name(base_ptr_->impl));
  }

  /**
   * @brief 检查执行器是否线程安全
   * @return 如果线程安全返回true,否则返回false
   */
  bool ThreadSafe() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return base_ptr_->is_thread_safe(base_ptr_->impl);
  }

  /**
   * @brief 检查当前线程是否为执行器线程
   * @return 如果是执行器线程返回true,否则返回false
   */
  bool IsInCurrentExecutor() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return base_ptr_->is_in_current_executor(base_ptr_->impl);
  }

  /**
   * @brief 检查是否支持定时调度功能
   * @return 如果支持定时调度返回true,否则返回false
   */
  bool SupportTimerSchedule() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return base_ptr_->is_support_timer_schedule(base_ptr_->impl);
  }

  /**
   * @brief 执行任务
   * @param task 待执行的任务
   */
  void Execute(Task&& task) {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    base_ptr_->execute(base_ptr_->impl, task.NativeHandle());
  }

  /**
   * @brief 获取执行器当前时间
   * @return 系统时钟时间点
   */
  std::chrono::system_clock::time_point Now() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return common::util::GetTimePointFromTimestampNs(base_ptr_->now(base_ptr_->impl));
  }

  /**
   * @brief 在指定时间点执行任务
   * @param tp 指定的执行时间点
   * @param task 待执行的任务
   */
  void ExecuteAt(std::chrono::system_clock::time_point tp, Task&& task) {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    AIMRT_ASSERT(SupportTimerSchedule(), "Current executor does not support timer scheduling.");
    base_ptr_->execute_at_ns(
        base_ptr_->impl, common::util::GetTimestampNs(tp), task.NativeHandle());
  }

  /**
   * @brief 延迟指定时间后执行任务
   * @param dt 延迟执行的时间间隔
   * @param task 待执行的任务
   */
  void ExecuteAfter(std::chrono::nanoseconds dt, Task&& task) {
    ExecuteAt(
        Now() + std::chrono::duration_cast<std::chrono::system_clock::time_point::duration>(dt),
        std::move(task));
  }

 private:
  /** @brief 底层执行器基类指针 */
  const aimrt_executor_base_t* base_ptr_ = nullptr;
};

}  // namespace aimrt::executor
