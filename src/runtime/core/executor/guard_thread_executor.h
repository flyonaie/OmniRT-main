// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

#include "aimrt_module_cpp_interface/executor/executor.h"
#include "aimrt_module_cpp_interface/util/string.h"
#include "util/log_util.h"

#include "yaml-cpp/yaml.h"

namespace aimrt::runtime::core::executor {

class GuardThreadExecutor {
 public:
  struct Options {
    std::string name = "aimrt_guard";
    std::string thread_sched_policy;
    std::vector<uint32_t> thread_bind_cpu;
    uint32_t queue_threshold = 10000; // 从10000调整为100（DS R1建议的，后面待确认）
  };
// 修改建议说明：
// 1. 阈值从10000调整为100，原因：
//    - Guard线程用于关键任务，队列积压超过100说明系统已过载，需立即告警
//    - 防止内存膨胀（每个任务约占用512B，100个任务约50KB，10000则需5MB）
//    - 与同类执行器配置对齐（如MainThreadExecutor默认50）
// 2. 添加阈值监控机制

  enum class State : uint32_t {
    kPreInit,
    kInit,
    kStart,
    kShutdown,
  };

 public:
  GuardThreadExecutor()
      : logger_ptr_(std::make_shared<aimrt::common::util::LoggerWrapper>()),
        base_(GenBase(this)) {}
  ~GuardThreadExecutor() = default;

  void Initialize(YAML::Node options_node);
  void Start();
  void Shutdown();

  std::string_view Type() const { return "guard_thread"; }
  std::string_view Name() const { return name_; }

  bool ThreadSafe() const { return true; }
  bool IsInCurrentExecutor() const {
    return std::this_thread::get_id() == thread_id_;
  }
  bool SupportTimerSchedule() const { return false; }

  void Execute(aimrt::executor::Task&& task);

  size_t CurrentTaskNum() noexcept { return queue_task_num_.load(); }

  State GetState() const { return state_.load(); }

  void SetLogger(const std::shared_ptr<aimrt::common::util::LoggerWrapper>& logger_ptr) { logger_ptr_ = logger_ptr; }
  const aimrt::common::util::LoggerWrapper& GetLogger() const { return *logger_ptr_; }

  const aimrt_executor_base_t* NativeHandle() const { return &base_; }

 private:
  static aimrt_executor_base_t GenBase(void* impl) {
    return aimrt_executor_base_t{
        .type = [](void* impl) -> aimrt_string_view_t {
          return aimrt::util::ToAimRTStringView(
              static_cast<GuardThreadExecutor*>(impl)->Type());
        },
        .name = [](void* impl) -> aimrt_string_view_t {
          return aimrt::util::ToAimRTStringView(
              static_cast<GuardThreadExecutor*>(impl)->Name());
        },
        .is_thread_safe = [](void* impl) -> bool {
          return static_cast<GuardThreadExecutor*>(impl)->ThreadSafe();
        },
        .is_in_current_executor = [](void* impl) -> bool {
          return static_cast<GuardThreadExecutor*>(impl)->IsInCurrentExecutor();
        },
        .is_support_timer_schedule = [](void* impl) -> bool {
          return static_cast<GuardThreadExecutor*>(impl)->SupportTimerSchedule();
        },
        .execute = [](void* impl, aimrt_function_base_t* task) {
          static_cast<GuardThreadExecutor*>(impl)->Execute(aimrt::executor::Task(task));  //
        },
        .now = nullptr,            // TODO, support time schedule
        .execute_at_ns = nullptr,  // TODO, support time schedule
        .impl = impl};
  }

 private:
  Options options_;
  std::atomic<State> state_ = State::kPreInit;
  std::shared_ptr<aimrt::common::util::LoggerWrapper> logger_ptr_;

  std::string name_;
  std::thread::id thread_id_;

  uint32_t queue_threshold_;
  uint32_t queue_warn_threshold_;
  std::atomic_uint32_t queue_task_num_ = 0;

  std::mutex mutex_;
  std::condition_variable cond_;
  std::queue<aimrt::executor::Task> queue_;
  std::unique_ptr<std::thread> thread_ptr_;

  const aimrt_executor_base_t base_;
};

}  // namespace aimrt::runtime::core::executor