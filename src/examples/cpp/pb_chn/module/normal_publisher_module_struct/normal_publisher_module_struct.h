// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <atomic>
#include <future>

#include "aimrt_module_cpp_interface/module_base.h"
#include "aimrt_module_cpp_interface/channel/channel_handle.h"
#include "aimrt_module_cpp_interface/channel/channel_context.h"

namespace aimrt::examples::cpp::pb_chn::normal_publisher_module_struct {

class NormalPublisherModule : public aimrt::ModuleBase {
 public:
  NormalPublisherModule() = default;
  ~NormalPublisherModule() override = default;

  ModuleInfo Info() const override {
    return ModuleInfo{.name = "NormalPublisherModule"};
  }

  bool Initialize(aimrt::CoreRef core) override;

  bool Start() override;

  void Shutdown() override;

 private:
  auto GetLogger() { return core_.GetLogger(); }

  void MainLoop();

 private:
  aimrt::CoreRef core_;
  aimrt::executor::ExecutorRef executor_;

  std::atomic_bool run_flag_ = true;
  std::promise<void> stop_sig_;

  std::string topic_name_ = "test_topic";
  double channel_frq_ = 0.5;
  aimrt::channel::PublisherRef publisher_;
};

}  // namespace aimrt::examples::cpp::pb_chn::normal_publisher_module_struct
