// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include <csignal>
#include <iostream>

#include "core/aimrt_core.h"
#include "normal_publisher_module/normal_publisher_module.h"

using namespace aimrt::runtime::core;
using namespace aimrt::examples::cpp::pb_chn::normal_publisher_module;

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

  std::cout << "AimRT start." << std::endl;

  try {
    AimRTCore core;
    global_core_ptr = &core;

    // register module
    NormalPublisherModule normal_publisher_module;
    core.GetModuleManager().RegisterModule(normal_publisher_module.NativeHandle());

    AimRTCore::Options options;
    if (argc > 1) options.cfg_file_path = argv[1];

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
