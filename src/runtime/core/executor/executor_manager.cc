// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include "core/executor/executor_manager.h"
#include "aimrt_module_cpp_interface/util/string.h"
#include "core/executor/asio_strand_executor.h"
#include "core/executor/asio_thread_executor.h"
#include "core/executor/simple_thread_executor.h"
// #define TBB_THREAD_EXECUTOR
#ifdef TBB_THREAD_EXECUTOR
#include "core/executor/tbb_thread_executor.h"
#endif
#include "core/executor/time_wheel_executor.h"
#include "util/string_util.h"


namespace YAML {

/**
 * @brief YAML转换器，用于将ExecutorManager::Options编码和解码为YAML节点。
 */
template <>
struct convert<aimrt::runtime::core::executor::ExecutorManager::Options> {
  using Options = aimrt::runtime::core::executor::ExecutorManager::Options;

  /**
   * @brief 将Options对象编码为YAML节点。
   * @param rhs 要编码的Options对象。
   * @return 编码后的YAML节点。
   */
  static Node encode(const Options& rhs) {
    Node node;

    node["executors"] = YAML::Node();
    for (const auto& executor : rhs.executors_options) {
      Node executor_node;
      executor_node["name"] = executor.name;
      executor_node["type"] = executor.type;
      executor_node["options"] = executor.options;
      node["executors"].push_back(executor_node);
    }

    return node;
  }

  /**
   * @brief 从YAML节点解码为Options对象。
   * @param node 要解码的YAML节点。
   * @param rhs 解码后的Options对象。
   * @return 如果解码成功返回true，否则返回false。
   */
  static bool decode(const Node& node, Options& rhs) {
    if (!node.IsMap()) return false;

    if (node["executors"] && node["executors"].IsSequence()) {
      for (const auto& executor_node : node["executors"]) {
        auto executor_options = Options::ExecutorOptions{
            .name = executor_node["name"].as<std::string>(),
            .type = executor_node["type"].as<std::string>()};

        if (executor_node["options"])
          executor_options.options = executor_node["options"];
        else
          executor_options.options = YAML::Node(YAML::NodeType::Null);

        rhs.executors_options.emplace_back(std::move(executor_options));
      }
    }

    return true;
  }
};

}  // namespace YAML

namespace aimrt::runtime::core::executor {

/**
 * @brief 初始化ExecutorManager。
 * @param options_node 包含初始化选项的YAML节点。
 */
void ExecutorManager::Initialize(YAML::Node options_node) {
  RegisterAsioExecutorGenFunc();
#ifdef TBB_THREAD_EXECUTOR
  RegisterTBBExecutorGenFunc();
#endif
  RegisterSimpleThreadExecutorGenFunc();
  RegisterTImeWheelExecutorGenFunc();

  AIMRT_CHECK_ERROR_THROW(
      std::atomic_exchange(&state_, State::kInit) == State::kPreInit,
      "Executor manager can only be initialized once.");

  if (options_node && !options_node.IsNull())
    options_ = options_node.as<Options>();

  // 生成executor
  for (auto& executor_options : options_.executors_options) {
    AIMRT_CHECK_ERROR_THROW(
        (executor_proxy_map_.find(executor_options.name) == executor_proxy_map_.end()) &&
            (used_executor_names_.find(executor_options.name) == used_executor_names_.end()),
        "Duplicate executor name '{}'.", executor_options.name);

    auto finditr = executor_gen_func_map_.find(executor_options.type);
    AIMRT_CHECK_ERROR_THROW(finditr != executor_gen_func_map_.end(),
                            "Invalid executor type '{}'.",
                            executor_options.type);

    auto executor_ptr = finditr->second();
    AIMRT_CHECK_ERROR_THROW(
        executor_ptr,
        "Gen executor failed, executor name '{}', executor type '{}'.",
        executor_options.name, executor_options.type);

    executor_ptr->Initialize(executor_options.name, executor_options.options);

    AIMRT_TRACE("Gen executor '{}' success.", executor_ptr->Name());

    auto proxy_ptr = std::make_unique<ExecutorProxy>(executor_ptr.get());

    executor_proxy_map_.emplace(executor_options.name, std::move(proxy_ptr));

    executor_vec_.emplace_back(std::move(executor_ptr));
  }

  options_node = options_;

  AIMRT_INFO("Executor manager init complete");
}

/**
 * @brief 启动ExecutorManager。
 */
void ExecutorManager::Start() {
  AIMRT_CHECK_ERROR_THROW(
      std::atomic_exchange(&state_, State::kStart) == State::kInit,
      "Method can only be called when state is 'Init'.");

  for (auto& itr : executor_vec_) {
    itr->Start();
  }

  AIMRT_INFO("Executor manager start completed.");
}

/**
 * @brief 关闭ExecutorManager。
 */
void ExecutorManager::Shutdown() {
  if (std::atomic_exchange(&state_, State::kShutdown) == State::kShutdown)
    return;

  AIMRT_INFO("Executor manager shutdown.");

  executor_manager_proxy_map_.clear();
  executor_proxy_map_.clear();

  for (auto& itr : executor_vec_) {
    itr->Shutdown();
  }

  executor_vec_.clear();
  executor_gen_func_map_.clear();
}

/**
 * @brief 注册Executor生成函数。
 * @param type Executor的类型。
 * @param executor_gen_func 生成Executor的函数。
 */
void ExecutorManager::RegisterExecutorGenFunc(
    std::string_view type, ExecutorGenFunc&& executor_gen_func) {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kPreInit,
      "Method can only be called when state is 'PreInit'.");

  executor_gen_func_map_.emplace(type, std::move(executor_gen_func));
}

/**
 * @brief 获取ExecutorManager的代理。
 * @param module_info 模块的详细信息。
 * @return ExecutorManager的代理。
 */
const ExecutorManagerProxy& ExecutorManager::GetExecutorManagerProxy(
    const util::ModuleDetailInfo& module_info) {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kInit,
      "Method can only be called when state is 'Init'.");

  auto itr = executor_manager_proxy_map_.find(module_info.name);
  if (itr != executor_manager_proxy_map_.end()) return *(itr->second);

  auto proxy_ptr = std::make_unique<ExecutorManagerProxy>(executor_proxy_map_);
  proxy_ptr->SetLogger(logger_ptr_);

  auto emplace_ret = executor_manager_proxy_map_.emplace(module_info.name, std::move(proxy_ptr));

  return *(emplace_ret.first->second);
}

/**
 * @brief 获取指定名称的Executor。
 * @param executor_name Executor的名称。
 * @return Executor的引用。
 */
aimrt::executor::ExecutorRef ExecutorManager::GetExecutor(
    std::string_view executor_name) {
  auto finditr = executor_proxy_map_.find(std::string(executor_name));
  if (finditr != executor_proxy_map_.end())
    return aimrt::executor::ExecutorRef(finditr->second->NativeHandle());

  AIMRT_WARN("Get executor failed, executor name '{}'", executor_name);

  return aimrt::executor::ExecutorRef();
}

/**
 * @brief 注册AsioExecutor生成函数。
 */
void ExecutorManager::RegisterAsioExecutorGenFunc() {
  RegisterExecutorGenFunc("asio_thread", [this]() -> std::unique_ptr<ExecutorBase> {
    auto ptr = std::make_unique<AsioThreadExecutor>();
    ptr->SetLogger(logger_ptr_);
    return ptr;
  });

  RegisterExecutorGenFunc("asio_strand", [this]() -> std::unique_ptr<ExecutorBase> {
    auto ptr = std::make_unique<AsioStrandExecutor>();
    ptr->SetLogger(logger_ptr_);
    ptr->RegisterGetAsioHandle(
        [this](std::string_view name) -> asio::io_context* {
          auto itr = std::find_if(
              executor_vec_.begin(),
              executor_vec_.end(),
              [name](const std::unique_ptr<ExecutorBase>& executor) -> bool {
                return (executor->Type() == "asio_thread") &&
                       (executor->Name() == name);
              });

          if (itr != executor_vec_.end())
            return dynamic_cast<AsioThreadExecutor*>(itr->get())->IOCTX();

          return nullptr;
        });
    return ptr;
  });
}

/**
 * @brief 注册TBBExecutor生成函数。
 */
#ifdef TBB_THREAD_EXECUTOR
void ExecutorManager::RegisterTBBExecutorGenFunc() {
  RegisterExecutorGenFunc("tbb_thread", [this]() -> std::unique_ptr<ExecutorBase> {
    auto ptr = std::make_unique<TBBThreadExecutor>();
    ptr->SetLogger(logger_ptr_);
    return ptr;
  });
}
#endif

/**
 * @brief 注册SimpleThreadExecutor生成函数。
 */
void ExecutorManager::RegisterSimpleThreadExecutorGenFunc() {
  RegisterExecutorGenFunc("simple_thread", [this]() -> std::unique_ptr<ExecutorBase> {
    auto ptr = std::make_unique<SimpleThreadExecutor>();
    ptr->SetLogger(logger_ptr_);
    return ptr;
  });
}

/**
 * @brief 注册TimeWheelExecutor生成函数。
 */
void ExecutorManager::RegisterTImeWheelExecutorGenFunc() {
  RegisterExecutorGenFunc("time_wheel", [this]() -> std::unique_ptr<ExecutorBase> {
    auto ptr = std::make_unique<TimeWheelExecutor>();
    ptr->SetLogger(logger_ptr_);
    ptr->RegisterGetExecutorFunc([this](std::string_view name) {
      return GetExecutor(name);
    });
    return ptr;
  });
}

/**
 * @brief 生成初始化报告。
 * @return 包含初始化信息的报告列表。
 */
std::list<std::pair<std::string, std::string>> ExecutorManager::GenInitializationReport() const {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kInit,
      "Method can only be called when state is 'Init'.");

  std::vector<std::string> executor_type_vec;
  for (const auto& itr : executor_gen_func_map_) {
    executor_type_vec.emplace_back(itr.first);
  }

  std::string executor_type_name_list;
  if (executor_type_vec.empty()) {
    executor_type_name_list = "<empty>";
  } else {
    executor_type_name_list = "[ " + aimrt::common::util::JoinVec(executor_type_vec, " , ") + " ]";
  }

  std::vector<std::vector<std::string>> executor_info_table =
      {{"name", "type", "thread safe", "support time schedule"}};

  for (const auto& item : executor_vec_) {
    std::vector<std::string> cur_executor_info(4);
    cur_executor_info[0] = item->Name();
    cur_executor_info[1] = item->Type();
    cur_executor_info[2] = item->ThreadSafe() ? "Y" : "N";
    cur_executor_info[3] = item->SupportTimerSchedule() ? "Y" : "N";
    executor_info_table.emplace_back(std::move(cur_executor_info));
  }

  std::list<std::pair<std::string, std::string>> report{
      {"Executor Type List", executor_type_name_list},
      {"Executor List", aimrt::common::util::DrawTable(executor_info_table)}};

  for (const auto& backend_ptr : executor_vec_) {
    report.splice(report.end(), backend_ptr->GenInitializationReport());
  }

  return report;
}

}  // namespace aimrt::runtime::core::executor
