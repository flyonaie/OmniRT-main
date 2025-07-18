// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include "core/executor/main_thread_executor.h"
#include "core/util/thread_tools.h"
#include <iostream>

namespace YAML {
template <>
struct convert<aimrt::runtime::core::executor::MainThreadExecutor::Options> {
  using Options = aimrt::runtime::core::executor::MainThreadExecutor::Options;

  static Node encode(const Options& rhs) {
    Node node;

    node["name"] = rhs.name;
    node["thread_sched_policy"] = rhs.thread_sched_policy;
    node["thread_bind_cpu"] = rhs.thread_bind_cpu;

    return node;
  }

  static bool decode(const Node& node, Options& rhs) {
    std::cout << "11111-decode: " << node << std::endl;
    if (!node.IsMap()) return false;
    std::cout << "22222-decode: " << node << std::endl;
    if (node["name"])
      rhs.name = node["name"].as<std::string>();
    std::cout << "33333-decode: " << node << std::endl;
    if (node["thread_sched_policy"])
      rhs.thread_sched_policy = node["thread_sched_policy"].as<std::string>();
    std::cout << "44444-decode: " << node << std::endl;
    
    if (node["thread_bind_cpu"]) {
      std::cout << "thread_bind_cpu exists" << std::endl;
      std::cout << "IsNull: " << node["thread_bind_cpu"].IsNull() << std::endl;
      std::cout << "IsSequence: " << node["thread_bind_cpu"].IsSequence() << std::endl;
      std::cout << "IsScalar: " << node["thread_bind_cpu"].IsScalar() << std::endl;
    }
    
    if (node["thread_bind_cpu"] && !node["thread_bind_cpu"].IsNull() && node["thread_bind_cpu"].IsSequence())
      rhs.thread_bind_cpu = node["thread_bind_cpu"].as<std::vector<uint32_t>>();
    std::cout << "55555-decode: " << node << std::endl;

    return true;
  }
};
}  // namespace YAML

namespace aimrt::runtime::core::executor {

void MainThreadExecutor::Initialize(YAML::Node options_node) {
  AIMRT_CHECK_ERROR_THROW(
      std::atomic_exchange(&state_, State::kInit) == State::kPreInit,
      "Main thread executor can only be initialized once.");

  if (options_node && !options_node.IsNull())
  {
    std::cout << "12345options_node: " << options_node << std::endl;
    options_ = options_node.as<Options>(); // 内部会调用decode函数
    std::cout << "999999options_.name: " << options_.name << std::endl;
  }
  std::cout << "options_.name: " << options_.name << std::endl;

  std::cout << "options_.thread_sched_policy: " << options_.thread_sched_policy << std::endl;
  for (const uint32_t& value : options_.thread_bind_cpu) 
  {
      std::cout << value << " ";
  }
  std::cout << std::endl;
 
  name_ = options_.name;

  try {
    util::SetNameForCurrentThread(name_);
    util::BindCpuForCurrentThread(options_.thread_bind_cpu);
    util::SetCpuSchedForCurrentThread(options_.thread_sched_policy);
  } catch (const std::exception& e) {
    AIMRT_WARN("Set thread policy for main thread get exception, {}", e.what());
  }

  main_thread_id_ = std::this_thread::get_id();

  // Options类可能包含默认值。当反序列化时，YAML节点可能不包含所有字段，这时Options对象会用默认值填充。将Options对象序列化回去，可以生成一个包含所有字段（包括默认值）的完整YAML节点，方便后续使用或持久化。
  options_node = options_;// 将Options对象赋值给YAML::Node,赋值运算符重载

  AIMRT_INFO("Main thread executor init complete");
}

void MainThreadExecutor::Start() {
  AIMRT_CHECK_ERROR_THROW(
      std::atomic_exchange(&state_, State::kStart) == State::kInit,
      "Main thread executor can only run when state is 'Init'.");
}

void MainThreadExecutor::Shutdown() {
  if (std::atomic_exchange(&state_, State::kShutdown) == State::kShutdown)
    return;

  AIMRT_INFO("Main thread executor shutdown.");
}

}  // namespace aimrt::runtime::core::executor
