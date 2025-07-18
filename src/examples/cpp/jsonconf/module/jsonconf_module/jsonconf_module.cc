// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include "yaml-cpp/yaml.h"
#include "nlohmann/json.hpp"
#include "jsonconf_module/jsonconf_module.h"
#include "MachineParamsTypeDef.h"

namespace aimrt::examples::cpp::jsonconf::jsonconf_module {

bool JsonconfModule::Initialize(aimrt::CoreRef core) {
  // Save aimrt framework handle
  core_ = core;

  // Read cfg
  auto file_path = core_.GetConfigurator().GetConfigFilePath();
  if (!file_path.empty()) {
    YAML::Node cfg_node = YAML::LoadFile(std::string(file_path));
    for (const auto& itr : cfg_node) {
      std::string k = itr.first.as<std::string>();
      std::string v = itr.second.as<std::string>();
      AIMRT_INFO("cfg [{} : {}]", k, v);
    }
  }

  JsonConf jsonconf("./mptd.json");
  std::string file_dump = jsonconf.FileDump();
  std::cout << "----------####jsonconf-file_dump: " << file_dump << std::endl;
  AIMRT_INFO("jsonconf-file_dump: {}", file_dump);

  // robot_bodyparams::S_JOINT_PARAMS joint_params;
  // auto joint_params = jsonconf.get<robot_bodyparams::S_JOINT_PARAMS>("stJoint");
  // AIMRT_INFO("joint_params: {}", joint_params);

  AIMRT_INFO("Init succeeded.");

  return true;
}

bool JsonconfModule::Start() {
  AIMRT_INFO("Start succeeded.");

  return true;
}

void JsonconfModule::Shutdown() {
  AIMRT_INFO("Shutdown succeeded.");
}
}  // namespace aimrt::examples::cpp::jsonconf::jsonconf_module
