// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include "executor_module/executor_module.h"

#include "yaml-cpp/yaml.h"
#include <iostream>

namespace aimrt::examples::cpp::executor::executor_module {

bool ExecutorModule::Initialize(aimrt::CoreRef core) {
  AIMRT_INFO("ExecutorModule initialization starting...");
  
  // Save aimrt framework handle
  core_ = core;
  
  // 验证core是否有效
  if (!core_) {
    AIMRT_ERROR("Invalid core reference provided");
    return false;
  }
  AIMRT_INFO("Core reference saved successfully");

  // Get executor
  AIMRT_INFO("Getting work_executor...");
  work_executor_ = core_.GetExecutorManager().GetExecutor("work_executor");
  AIMRT_INFO("work_executor: {}", work_executor_ ? "valid" : "invalid");
  if (!work_executor_) {
    AIMRT_ERROR("Can not get work_executor");
    return false;
  }
  AIMRT_INFO("Got work_executor successfully");

  // Get thread safe executor
  // AIMRT_INFO("Getting thread_safe_executor...");
  // thread_safe_executor_ = core_.GetExecutorManager().GetExecutor("thread_safe_executor");
  // AIMRT_INFO("thread_safe_executor: {}", thread_safe_executor_ ? "valid" : "invalid");
  // if (!thread_safe_executor_ || !thread_safe_executor_.ThreadSafe()) {
  //   AIMRT_ERROR("Can not get thread_safe_executor");
  //   return false;
  // }
  // AIMRT_INFO("Got thread_safe_executor successfully");

  // Get time schedule executor
  AIMRT_INFO("Getting time_schedule_executor...");
  time_schedule_executor_ = core_.GetExecutorManager().GetExecutor("time_schedule_executor");
  AIMRT_INFO("time_schedule_executor: {}", time_schedule_executor_ ? "valid" : "invalid");
  if (!time_schedule_executor_ || !time_schedule_executor_.SupportTimerSchedule()) {
    AIMRT_ERROR("Can not get time_schedule_executor");
    return false;
  }
  AIMRT_INFO("Got time_schedule_executor successfully");

  AIMRT_INFO("ExecutorModule::Initialize success");
  AIMRT_INFO("Init succeeded.");

  return true;
}

bool ExecutorModule::Start() {
  // Test simple execute
  AIMRT_INFO("Starting SimpleExecuteDemo...");
  SimpleExecuteDemo();
  AIMRT_INFO("SimpleExecuteDemo completed.");

  // Test thread safe execute
  AIMRT_INFO("Starting ThreadSafeDemo...");
  // ThreadSafeDemo();
  AIMRT_INFO("ThreadSafeDemo completed.");

  // Test time schedule execute
  AIMRT_INFO("Starting TimeScheduleDemo...");
  TimeScheduleDemo();
  AIMRT_INFO("TimeScheduleDemo completed.");

  AIMRT_INFO("Start succeeded.");

  return true;
}

void ExecutorModule::Shutdown() {
  AIMRT_INFO("Shutting down...");
  run_flag_ = false;

  std::this_thread::sleep_for(std::chrono::seconds(1));

  AIMRT_INFO("Shutdown succeeded.");
}

void ExecutorModule::SimpleExecuteDemo() {
  work_executor_.Execute([this]() {
    AIMRT_INFO("This is a simple task");
  });
}

void ExecutorModule::ThreadSafeDemo() {
  uint32_t n = 0;
  for (uint32_t ii = 0; ii < 10000; ++ii) {
    thread_safe_executor_.Execute([&n]() {
      n++;
    });
  }

  std::this_thread::sleep_for(std::chrono::seconds(1));

  AIMRT_INFO("Value of n is {}", n);
}

void ExecutorModule::TimeScheduleDemo() {
  if (!run_flag_) return;

  AIMRT_INFO("Loop count : {}", loop_count_++);

  time_schedule_executor_.ExecuteAfter(
      std::chrono::seconds(1),
      std::bind(&ExecutorModule::TimeScheduleDemo, this));
}

}  // namespace aimrt::examples::cpp::executor::executor_module
