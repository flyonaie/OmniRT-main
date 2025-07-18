// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "core/executor/executor_base.h"
#include "util/log_util.h"

#include "yaml-cpp/yaml.h"

namespace aimrt::runtime::core::executor {

/**
 * @class NativeThreadExecutor
 * @brief 基于C++11标准库的线程执行器实现
 * @details 该执行器提供了基于标准库的异步任务处理能力，支持：
 *          - 多线程并行处理
 *          - 任务队列监控
 *          - CPU绑定和调度策略设置
 *          - 定时任务执行
 */
class NativeThreadExecutor : public ExecutorBase {
 public:
  struct Options {
    uint32_t thread_num = 1;
    std::string thread_sched_policy;
    std::vector<uint32_t> thread_bind_cpu;
    std::chrono::nanoseconds timeout_alarm_threshold_us = std::chrono::seconds(1);
    uint32_t queue_threshold = 10000;
  };

  enum class State : uint32_t {
    kPreInit,
    kInit,
    kStart,
    kShutdown,
  };

 public:
  NativeThreadExecutor()
      : logger_ptr_(std::make_shared<aimrt::common::util::LoggerWrapper>()) {}
  ~NativeThreadExecutor() override;

  void Initialize(std::string_view name, YAML::Node options_node) override;
  void Start() override;
  void Shutdown() override;

  std::string_view Type() const noexcept override { return "native_thread"; }
  std::string_view Name() const noexcept override { return name_; }

  bool ThreadSafe() const noexcept override { return (options_.thread_num == 1); }
  bool IsInCurrentExecutor() const noexcept override;
  bool SupportTimerSchedule() const noexcept override { return true; }

  void Execute(aimrt::executor::Task&& task) noexcept override;

  std::chrono::system_clock::time_point Now() const noexcept override {
    return std::chrono::system_clock::now();
  }
  void ExecuteAt(std::chrono::system_clock::time_point tp, aimrt::executor::Task&& task) noexcept override;

  size_t CurrentTaskNum() noexcept override { return queue_task_num_.load(); }

  State GetState() const { return state_.load(); }

  void SetLogger(const std::shared_ptr<aimrt::common::util::LoggerWrapper>& logger_ptr) { logger_ptr_ = logger_ptr; }
  const aimrt::common::util::LoggerWrapper& GetLogger() const { return *logger_ptr_; }

 private:
  void WorkerThread(uint32_t i);
  // void ProcessTimers();
  
  std::string name_;
  Options options_;
  std::atomic<State> state_ = State::kPreInit;
  std::shared_ptr<aimrt::common::util::LoggerWrapper> logger_ptr_;

  uint32_t queue_threshold_;
  uint32_t queue_warn_threshold_;
  std::atomic_uint32_t queue_task_num_ = 0;

  // 任务队列相关
  std::mutex queue_mutex_;
  std::condition_variable queue_condition_;
  std::deque<aimrt::executor::Task> task_queue_;
  
  // 定时任务相关
#if 0
  struct TimerTask {
    std::chrono::system_clock::time_point time_point;
    aimrt::executor::Task task;
    
    bool operator<(const TimerTask& other) const {
      return time_point > other.time_point; // 优先级队列需要最小的时间点在顶部
    }
  };
#endif
  
  std::mutex timer_mutex_;
  std::condition_variable timer_condition_;
  // std::vector<TimerTask> timer_tasks_; // 使用vector实现优先队列
  std::atomic_bool timer_updated_ = false;
  std::unique_ptr<std::thread> timer_thread_;

  // 工作线程
  std::vector<std::thread::id> thread_id_vec_;
  std::list<std::thread> threads_;
  std::atomic_bool running_ = false;
};

}  // namespace aimrt::runtime::core::executor