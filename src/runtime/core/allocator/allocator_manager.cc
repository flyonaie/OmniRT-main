// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include "core/allocator/allocator_manager.h"

namespace YAML {
template <>
struct convert<aimrt::runtime::core::allocator::AllocatorManager::Options> {
  using Options = aimrt::runtime::core::allocator::AllocatorManager::Options;

  // 序列化：将Options转换为YAML节点
  static Node encode(const Options& rhs) {
    Node node;
    return node;// 目前是空实现，为将来扩展预留
  }

  // 反序列化：将YAML节点转换为Options
  static bool decode(const Node& node, Options& rhs) {
    if (!node.IsMap()) return false;// 确保是Map类型的节点

    return true;// 目前是空实现，为将来扩展预留
  }
};
}  // namespace YAML

namespace aimrt::runtime::core::allocator {

void AllocatorManager::Initialize(YAML::Node options_node) {
  AIMRT_CHECK_ERROR_THROW(
      std::atomic_exchange(&state_, State::kInit) == State::kPreInit,
      "AllocatorManager manager can only be initialized once.");

  if (options_node && !options_node.IsNull())
    options_ = options_node.as<Options>();//将节点 options_node 转换为指定类型 Options

  options_node = options_;

  AIMRT_INFO("Allocator manager init complete");
}

void AllocatorManager::Start() {
  AIMRT_CHECK_ERROR_THROW(
      std::atomic_exchange(&state_, State::kStart) == State::kInit,
      "Method can only be called when state is 'Init'.");

  AIMRT_INFO("Allocator manager start completed.");
}

void AllocatorManager::Shutdown() {
  if (std::atomic_exchange(&state_, State::kShutdown) == State::kShutdown)
    return;

  AIMRT_INFO("Allocator manager shutdown.");
}

const AllocatorProxy& AllocatorManager::GetAllocatorProxy(const util::ModuleDetailInfo& module_info) {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kInit,
      "Method can only be called when state is 'Init'.");

  return allocator_proxy_;
}

std::list<std::pair<std::string, std::string>> AllocatorManager::GenInitializationReport() const {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kInit,
      "Method can only be called when state is 'Init'.");

  return {};
}

}  // namespace aimrt::runtime::core::allocator
