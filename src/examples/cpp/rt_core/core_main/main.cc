// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include <csignal>
#include <iostream>

#include "core/main/core_main.h"

using namespace aimrt::runtime::core;

int32_t main(int32_t argc, char** argv) {
 
  std::cout << "AimRT start..." << std::endl;

  AimRTCoreMain core_main;
  AimRTCoreMain::Options options;
  
  core_main.Initialize(options);
  core_main.Start();
  core_main.Shutdown();

  std::cout << "AimRT exit." << std::endl;
  return 0;
}
