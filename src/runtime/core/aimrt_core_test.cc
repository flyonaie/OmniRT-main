// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "core/aimrt_core.h"

namespace aimrt::runtime::core {

TEST(AimRTCoreTest, BasicLifecycle) {
  AimRTCore core;
  AimRTCore::Options options;
  options.cfg_file_path = "config/aimrt_test.yaml";
  
  // Test initialization
  EXPECT_NO_THROW(core.Initialize(options));
  EXPECT_EQ(core.GetState(), AimRTCore::State::kPostInit);
  
  // Test async start
  auto future = core.AsyncStart();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(core.GetState(), AimRTCore::State::kPostStart);
  
  // Test shutdown
  core.Shutdown();
  future.wait();
  EXPECT_EQ(core.GetState(), AimRTCore::State::kPostShutdown);
}

}  // namespace aimrt::runtime::core
