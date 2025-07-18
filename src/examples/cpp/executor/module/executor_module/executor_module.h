// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <atomic>
#include <memory>

#include "aimrt_module_cpp_interface/module_base.h"

namespace aimrt::examples::cpp::executor::executor_module {

class ExecutorModule : public aimrt::ModuleBase {
 public:
  ExecutorModule() = default;
  ~ExecutorModule() override = default;

  ModuleInfo Info() const override {
    return ModuleInfo{
      .name = "ExecutorModule",
      .major_version = 1,
      .minor_version = 0,
      .patch_version = 0,
      .build_version = 0,
      .author = "AgiBot Inc.",
      .description = "A module for demonstrating executor functionality"
    };
  }

  bool Initialize(aimrt::CoreRef aimrt_ptr) override;

  bool Start() override;

  void Shutdown() override;

 private:
  /**
   * @brief 获取日志记录器
   * 
   * @return 如果core_已初始化则返回core_的logger，否则返回默认logger
   */
  auto GetLogger() { 
    if (core_) {
      return core_.GetLogger();
    }
    // 如果core_未初始化，返回一个默认的logger
    return aimrt::logger::GetSimpleLoggerRef();
  }

  void SimpleExecuteDemo();
  void ThreadSafeDemo();
  void TimeScheduleDemo();

 private:
  aimrt::CoreRef core_;

  aimrt::executor::ExecutorRef work_executor_;
  aimrt::executor::ExecutorRef thread_safe_executor_;

  std::atomic_bool run_flag_ = true;
  uint32_t loop_count_ = 0;
  aimrt::executor::ExecutorRef time_schedule_executor_;
};

}  // namespace aimrt::examples::cpp::executor::executor_module
