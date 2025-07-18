// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include <csignal>
#include <iostream>

#include "core/aimrt_core.h"
#include "executor_module/executor_module.h"

using namespace aimrt::runtime::core;
using namespace aimrt::examples::cpp::executor::executor_module;

AimRTCore* global_core_ptr = nullptr;

void SignalHandler(int sig) {
  if (global_core_ptr && (sig == SIGINT || sig == SIGTERM)) {
    global_core_ptr->Shutdown();
    return;
  }

  raise(sig);
};

int32_t main(int32_t argc, char** argv) {
  signal(SIGINT, SignalHandler);
  signal(SIGTERM, SignalHandler);

  std::cout << "AimRT start----------." << std::endl;

  try {
    AimRTCore core;
    global_core_ptr = &core;
    std::cout << "AimRT initialize:argc: " << argc << "." << std::endl;
       
    // register module
    ExecutorModule executor_module;
    std::cout << "AimRT initialize:executor_module.NativeHandle(): " << executor_module.NativeHandle() << "." << std::endl;
    core.GetModuleManager().RegisterModule(executor_module.NativeHandle());
    std::cout << "ssssssssssss." << std::endl;
    
    AimRTCore::Options options;
    if (argc > 1) options.cfg_file_path = argv[1];

    std::cout << "AimRT initialize:options.cfg_file_path: " << options.cfg_file_path << "." << std::endl;
    core.Initialize(options);

    core.Start();

    core.Shutdown();

    global_core_ptr = nullptr;
  } catch (const std::exception& e) {
    std::cout << "AimRT run with exception and exit. " << e.what()
              << std::endl;
    return -1;
  }

  std::cout << "AimRT exit." << std::endl;

  return 0;
}
