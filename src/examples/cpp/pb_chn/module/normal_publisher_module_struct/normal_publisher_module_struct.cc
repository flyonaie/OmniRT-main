// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include "normal_publisher_module_struct/normal_publisher_module_struct.h"
// #include "aimrt_module_protobuf_interface/channel/protobuf_channel.h"
// #include "aimrt_module_protobuf_interface/util/protobuf_tools.h"
#include "aimrt_module_cpp_interface/channel/channel_handle.h"
#include "yaml-cpp/yaml.h"

#include "normal_publisher_module_struct/datatype.h"
#include "normal_publisher_module_struct/struct_type_support.h"

namespace aimrt::examples::cpp::pb_chn::normal_publisher_module_struct {

bool NormalPublisherModule::Initialize(aimrt::CoreRef core) {
  core_ = core;

  try {
    // Read cfg
    auto file_path = core_.GetConfigurator().GetConfigFilePath();
    if (!file_path.empty()) {
      YAML::Node cfg_node = YAML::LoadFile(std::string(file_path));
      topic_name_ = cfg_node["topic_name"].as<std::string>();
      channel_frq_ = cfg_node["channel_frq"].as<double>();
    }

    // Get executor handle
    executor_ = core_.GetExecutorManager().GetExecutor("work_thread_pool");
    AIMRT_CHECK_ERROR_THROW(executor_ && executor_.SupportTimerSchedule(),
                            "Get executor 'work_thread_pool' failed.");

    // Register publish type
    publisher_ = core_.GetChannelHandle().GetPublisher(topic_name_);
    AIMRT_CHECK_ERROR_THROW(publisher_, "Get publisher for topic '{}' failed.", topic_name_);

    bool ret = aimrt::channel::RegisterPublishType<EventData>(publisher_);
    AIMRT_CHECK_ERROR_THROW(ret, "Register publish type failed.");

  } catch (const std::exception& e) {
    AIMRT_ERROR("Init failed, {}", e.what());
    return false;
  }

  AIMRT_INFO("Init succeeded.");

  return true;
}

bool NormalPublisherModule::Start() {
  try {
    executor_.Execute(std::bind(&NormalPublisherModule::MainLoop, this));
  } catch (const std::exception& e) {
    AIMRT_ERROR("Start failed, {}", e.what());
    return false;
  }

  AIMRT_INFO("Start succeeded.");
  return true;
}

void NormalPublisherModule::Shutdown() {
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
void NormalPublisherModule::MainLoop() {
  try {
    AIMRT_INFO("Start MainLoop.");

    aimrt::channel::PublisherProxy<struct EventData> publisher_proxy(publisher_);

  uint32_t count = 0;

  while (run_flag_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<uint32_t>(1000 / channel_frq_)));

      count++;
      AIMRT_INFO("Loop count : {} -------------------------", count);

      // publish event
      struct EventData msg;
      // msg.id("count: " + std::to_string(count));
      // msg.value(count);
      msg.id = count;
      msg.value = count * 0.1;

      AIMRT_INFO("Publish new pb event, data: id{}, value{}", msg.id, msg.value);
      publisher_proxy.Publish(msg);
    }

    AIMRT_INFO("Exit MainLoop.");
  } catch (const std::exception& e) {
    AIMRT_ERROR("Exit MainLoop with exception, {}", e.what());
  }

  stop_sig_.set_value();
}

}  // namespace aimrt::examples::cpp::pb_chn::normal_publisher_module_struct
