// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include <csignal>
#include <iostream>

// #include "aimrt_module_cpp_interface/core.h"
#include "core/aimrt_core.h"

using namespace aimrt::runtime::core;

AimRTCore* global_core_ptr = nullptr;

int32_t main(int32_t argc, char** argv) {
 
  std::cout << "AimRT start." << std::endl;
  AimRTCore core;
  AimRTCore::Options options;
  core.Initialize(options);

  std::cout << "AimRT exit." << std::endl;

  return 0;
}
