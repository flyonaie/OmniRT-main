// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include "normal_rpc_sync_client_module/normal_rpc_sync_client_module.h"
#include "aimrt_module_protobuf_interface/util/protobuf_tools.h"

#include "yaml-cpp/yaml.h"
#include <iostream>

namespace aimrt::examples::cpp::pb_rpc::normal_rpc_sync_client_module {

bool NormalRpcSyncClientModule::Initialize(aimrt::CoreRef core) {
  core_ = core;
  AIMRT_INFO("NormalRpcSyncClientModule::Initialize starting...");
 
  try {
    // Read cfg
    std::string file_path = std::string(core_.GetConfigurator().GetConfigFilePath());
    if (!file_path.empty()) {
      YAML::Node cfg_node = YAML::LoadFile(file_path);
      rpc_frq_ = cfg_node["rpc_frq"].as<double>();
    }

    // Get executor handle
    executor_ = core_.GetExecutorManager().GetExecutor("work_thread_pool");
    AIMRT_CHECK_ERROR_THROW(executor_, "Get executor 'work_thread_pool' failed.");

    // Get rpc handle
    auto rpc_handle = core_.GetRpcHandle();
    AIMRT_CHECK_ERROR_THROW(rpc_handle, "Get rpc handle failed.");

    // Register rpc client
    bool ret = aimrt::protocols::example::RegisterExampleServiceClientFunc(rpc_handle);
    AIMRT_CHECK_ERROR_THROW(ret, "Register client failed.");

  } catch (const std::exception& e) {
    AIMRT_ERROR("Init failed, {}", e.what());
    return false;
  }

  std::cout << "Start...111111111111" << std::endl;
  AIMRT_INFO("NormalRpcSyncClientModule Init succeeded.");
  return true;
}

bool NormalRpcSyncClientModule::Start() {
  // std::cout << "Start...111111111111" << std::endl;

  AIMRT_INFO("NormalRpcSyncClientModule starting...");
  try {
    if (!executor_) {
      AIMRT_ERROR("Executor is invalid");
      return false;
    }
    executor_.Execute(std::bind(&NormalRpcSyncClientModule::MainLoop, this));
  } catch (const std::exception& e) {
    AIMRT_ERROR("Start failed, {}", e.what());
    return false;
  }
  AIMRT_INFO("NormalRpcSyncClientModule started successfully");
  return true;
}

void NormalRpcSyncClientModule::Shutdown() {
  try {
    run_flag_ = false;
    stop_sig_.get_future().wait();
  } catch (const std::exception& e) {
    AIMRT_ERROR("Shutdown failed, {}", e.what());
    return;
  }

  AIMRT_INFO("Shutdown succeeded.");
}

// Main loop
void NormalRpcSyncClientModule::MainLoop() {
  try {
    AIMRT_INFO("Start MainLoop.");

    // Create proxy
    aimrt::protocols::example::ExampleServiceSyncProxy proxy(core_.GetRpcHandle());

    uint32_t count = 0;
    while (run_flag_) {
      // Sleep
      std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<uint32_t>(1000 / rpc_frq_)));

      count++;
      AIMRT_INFO("Loop count : {} -------------------------", count);

      // Create req and rsp
      aimrt::protocols::example::GetFooDataReq req;
      aimrt::protocols::example::GetFooDataRsp rsp;
      req.set_msg("hello world foo, count " + std::to_string(count));

      // Create ctx
      auto ctx_ptr = proxy.NewContextSharedPtr();
      ctx_ptr->SetTimeout(std::chrono::seconds(3));

      AIMRT_INFO("Client start new rpc call. req: {}", aimrt::Pb2CompactJson(req));

      // Call rpc
      auto status = proxy.GetFooData(ctx_ptr, req, rsp);

      // Check result
      if (status.OK()) {
        AIMRT_INFO("Client get rpc ret, status: {}, rsp: {}", status.ToString(),
                   aimrt::Pb2CompactJson(rsp));
      } else {
        AIMRT_WARN("Client get rpc error ret, status: {}", status.ToString());
      }
    }

    AIMRT_INFO("Exit MainLoop.");
  } catch (const std::exception& e) {
    AIMRT_ERROR("Exit MainLoop with exception, {}", e.what());
  }

  stop_sig_.set_value();
}

}  // namespace aimrt::examples::cpp::pb_rpc::normal_rpc_sync_client_module
