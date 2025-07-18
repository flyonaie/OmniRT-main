// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include <csignal>
#include <iostream>

#include "main/core_main.h"

using namespace aimrt::core;

int32_t main(int32_t argc, char** argv) {
 
  std::cout << "AimRT start..." << std::endl;

  AimRTCoreMain core_main;
  AimRTCoreMain::Options options;
  
  if (!core_main.Initialize(options)) {
    std::cout << "Failed to initialize core" << std::endl;
    return -1;
  }

  if (!core_main.Start()) {
    std::cout << "Failed to start core" << std::endl;
    return -1;
  }

  core_main.Shutdown();

  std::cout << "AimRT exit." << std::endl;
  return 0;
}
