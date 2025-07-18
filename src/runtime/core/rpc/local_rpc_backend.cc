// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

/**
 * @file local_rpc_backend.cc
 * @brief 本地RPC后端实现，用于处理本地服务间的RPC调用
 * @details 实现了LocalRpcBackend类，提供本地进程内RPC服务的注册、发现和调用功能
 */

#include "core/rpc/local_rpc_backend.h"
#include "core/rpc/rpc_backend_tools.h"
#include "util/string_util.h"
#include "util/url_parser.h"
#include "util/macros.h"

/**
 * @namespace YAML
 * @brief YAML序列化/反序列化命名空间
 */
namespace YAML {
/**
 * @brief LocalRpcBackend::Options的YAML转换特化实现
 * @details 实现了Options结构的序列化和反序列化功能
 */
template <>
struct convert<aimrt::runtime::core::rpc::LocalRpcBackend::Options> {
  using Options = aimrt::runtime::core::rpc::LocalRpcBackend::Options;

  /**
   * @brief 将Options序列化为YAML节点
   * @param rhs 要序列化的Options对象
   * @return 序列化后的YAML节点
   */
  static Node encode(const Options& rhs) {
    Node node;

    node["timeout_executor"] = rhs.timeout_executor;

    return node;
  }

  /**
   * @brief 从YAML节点反序列化为Options对象
   * @param node 包含序列化数据的YAML节点
   * @param rhs 反序列化的目标Options对象
   * @return 反序列化是否成功
   */
  static bool decode(const Node& node, Options& rhs) {
    if (node["timeout_executor"])
      rhs.timeout_executor = node["timeout_executor"].as<std::string>();

    return true;
  }
};
}  // namespace YAML

/**
 * @namespace aimrt::runtime::core::rpc
 * @brief AIMRT运行时RPC核心功能命名空间
 */
namespace aimrt::runtime::core::rpc {

/**
 * @brief 初始化本地RPC后端
 * @param options_node 包含配置选项的YAML节点
 * @throw 当后端已经被初始化过时抛出异常
 * @details 根据配置初始化RPC后端，如果配置了超时执行器则启用超时功能
 */
void LocalRpcBackend::Initialize(YAML::Node options_node) {
  AIMRT_CHECK_ERROR_THROW(
      std::atomic_exchange(&state_, State::kInit) == State::kPreInit,
      "Local rpc backend can only be initialized once.");

  if (options_node && !options_node.IsNull())
    options_ = options_node.as<Options>();

  client_tool_ptr_ =
      std::make_unique<util::RpcClientTool<std::shared_ptr<InvokeWrapper>>>();

  if (!options_.timeout_executor.empty()) {
    AIMRT_CHECK_ERROR_THROW(
        get_executor_func_,
        "Get executor function is not set before initialize.");

    auto timeout_executor = get_executor_func_(options_.timeout_executor);

    AIMRT_CHECK_ERROR_THROW(
        timeout_executor,
        "Get timeout executor '{}' failed.", options_.timeout_executor);

    client_tool_ptr_->RegisterTimeoutExecutor(timeout_executor);
    client_tool_ptr_->RegisterTimeoutHandle(
        [](std::shared_ptr<InvokeWrapper>&& client_invoke_wrapper_ptr) {
          client_invoke_wrapper_ptr->callback(aimrt::rpc::Status(AIMRT_RPC_STATUS_TIMEOUT));
        });

    AIMRT_TRACE("Local rpc backend enable the timeout function, use '{}' as timeout executor.",
                options_.timeout_executor);
  } else {
    AIMRT_TRACE("Local rpc backend does not enable the timeout function.");
  }

  options_node = options_;
}

/**
 * @brief 启动本地RPC后端服务
 * @throw 当状态不是初始化状态时抛出异常
 * @details 将后端状态从初始化状态转换为启动状态
 */
void LocalRpcBackend::Start() {
  AIMRT_CHECK_ERROR_THROW(
      std::atomic_exchange(&state_, State::kStart) == State::kInit,
      "Method can only be called when state is 'Init'.");
}

/**
 * @brief 关闭本地RPC后端服务
 * @details 清理资源并将状态设置为关闭状态，释放注册表和客户端工具
 */
void LocalRpcBackend::Shutdown() {
  if (std::atomic_exchange(&state_, State::kShutdown) == State::kShutdown)
    return;

  service_func_register_index_.clear();

  client_tool_ptr_.reset();

  get_executor_func_ = std::function<executor::ExecutorRef(std::string_view)>();
}

/**
 * @brief 注册服务函数到本地RPC后端
 * @param service_func_wrapper 服务函数包装器，包含服务函数信息和实现
 * @return 注册是否成功
 * @details 在初始化状态下注册服务函数，构建服务索引表
 */
bool LocalRpcBackend::RegisterServiceFunc(
    const ServiceFuncWrapper& service_func_wrapper) noexcept {
  try {
    if (state_.load() != State::kInit) {
      AIMRT_ERROR("Service func can only be registered when state is 'Init'.");
      return false;
    }

    const auto& info = service_func_wrapper.info;

    service_func_register_index_[info.func_name][info.pkg_path].emplace(info.module_name);

    return true;
  } catch (const std::exception& e) {
    AIMRT_ERROR("{}", e.what());
    return false;
  }
}

/**
 * @brief 注册客户端函数到本地RPC后端
 * @param client_func_wrapper 客户端函数包装器，包含客户端函数信息
 * @return 注册是否成功
 * @details 在初始化状态下注册客户端函数
 */
bool LocalRpcBackend::RegisterClientFunc(
    const ClientFuncWrapper& client_func_wrapper) noexcept {
  try {
    if (state_.load() != State::kInit) {
      AIMRT_ERROR("Client func can only be registered when state is 'Init'.");
      return false;
    }

    return true;
  } catch (const std::exception& e) {
    AIMRT_ERROR("{}", e.what());
    return false;
  }
}

/**
 * @brief 执行RPC调用
 * @param client_invoke_wrapper_ptr 客户端调用包装器指针，包含请求信息
 * @details 处理RPC调用流程，包括：
 *   1. 解析目标地址获取服务包路径和模块名
 *   2. 查找匹配的服务函数
 *   3. 创建服务调用上下文
 *   4. 处理跨包调用时的序列化/反序列化
 *   5. 执行调用并处理回调
 */
void LocalRpcBackend::Invoke(
    const std::shared_ptr<InvokeWrapper>& client_invoke_wrapper_ptr) noexcept {
  try {
    if (omnirt_unlikely(state_.load() != State::kStart)) {
      AIMRT_WARN("Method can only be called when state is 'Start'.");
      client_invoke_wrapper_ptr->callback(aimrt::rpc::Status(AIMRT_RPC_STATUS_CLI_BACKEND_INTERNAL_ERROR));
      return;
    }

    // 检查上下文，获取服务路径信息
    std::string_view service_pkg_path, service_module_name;

    auto to_addr = client_invoke_wrapper_ptr->ctx_ref.GetMetaValue(AIMRT_RPC_CONTEXT_KEY_TO_ADDR);

    // URL格式: local://rpc/func_name?pkg_path=xxxx&module_name=yyyy
    if (!to_addr.empty()) {
      namespace util = aimrt::common::util;
      auto url = util::ParseUrl<std::string_view>(std::string(to_addr));
      if (url) {
        if (omnirt_unlikely(url->protocol != Name())) {
          AIMRT_WARN("Invalid addr: {}", to_addr);
          client_invoke_wrapper_ptr->callback(aimrt::rpc::Status(AIMRT_RPC_STATUS_CLI_BACKEND_INTERNAL_ERROR));
          return;
        }
        service_pkg_path = util::GetValueFromStrKV(url->query, "pkg_path");
        service_module_name = util::GetValueFromStrKV(url->query, "module_name");
      }
    }

    const auto& client_info = client_invoke_wrapper_ptr->info;

    // 从本地服务注册表中查找符合条件的服务函数
    auto service_func_register_index_find_func_itr = service_func_register_index_.find(client_info.func_name);
    if (omnirt_unlikely(service_func_register_index_find_func_itr == service_func_register_index_.end())) {
      AIMRT_ERROR("Service func '{}' is not registered in local rpc backend.", client_info.func_name);
      client_invoke_wrapper_ptr->callback(aimrt::rpc::Status(AIMRT_RPC_STATUS_SVR_NOT_FOUND));
      return;
    }

    if (service_pkg_path.empty()) {
      if (service_module_name.empty()) {
        // 包路径和模块名都未指定，使用第一个可用的服务
        auto service_func_register_index_find_pkg_itr =
            service_func_register_index_find_func_itr->second.begin();

        service_pkg_path = service_func_register_index_find_pkg_itr->first;
        service_module_name = *(service_func_register_index_find_pkg_itr->second.begin());
      } else {
        // 包路径未指定但模块名已指定，遍历所有包查找第一个包含该模块的包
        for (const auto& itr : service_func_register_index_find_func_itr->second) {
          if (itr.second.find(service_module_name) != itr.second.end()) {
            service_pkg_path = itr.first;
            break;
          }
        }
        if (service_pkg_path.empty()) {
          AIMRT_WARN("Can not find service func '{}' in module '{}'. Addr: {}",
                     client_info.func_name, service_module_name, to_addr);

          client_invoke_wrapper_ptr->callback(aimrt::rpc::Status(AIMRT_RPC_STATUS_CLI_INVALID_ADDR));
          return;
        }
      }

    } else {
      auto service_func_register_index_find_pkg_itr =
          service_func_register_index_find_func_itr->second.find(service_pkg_path);

      if (omnirt_unlikely(service_func_register_index_find_pkg_itr ==
          service_func_register_index_find_func_itr->second.end())) {
        AIMRT_WARN("Can not find service func '{}' in pkg '{}'. Addr: {}",
                   client_info.func_name, service_pkg_path, to_addr);

        client_invoke_wrapper_ptr->callback(aimrt::rpc::Status(AIMRT_RPC_STATUS_CLI_INVALID_ADDR));
        return;
      }

      if (service_module_name.empty()) {
        service_module_name = *(service_func_register_index_find_pkg_itr->second.begin());
      } else {
        auto service_func_register_index_find_module_itr =
            service_func_register_index_find_pkg_itr->second.find(service_module_name);

        if (omnirt_unlikely(service_func_register_index_find_module_itr ==
            service_func_register_index_find_pkg_itr->second.end())) {
          AIMRT_WARN("Can not find service func '{}' in pkg '{}' module '{}'. Addr: {}",
                     client_info.func_name, service_pkg_path, service_module_name, to_addr);

          client_invoke_wrapper_ptr->callback(aimrt::rpc::Status(AIMRT_RPC_STATUS_CLI_INVALID_ADDR));
          return;
        }
      }
    }

    AIMRT_TRACE("Invoke rpc func '{}' in pkg '{}' module '{}'.",
                client_info.func_name, service_pkg_path, service_module_name);

    // 获取已注册的服务函数实现
    const auto* service_func_wrapper_ptr =
        rpc_registry_ptr_->GetServiceFuncWrapperPtr(client_info.func_name, service_pkg_path, service_module_name);

    if (omnirt_unlikely(service_func_wrapper_ptr == nullptr)) {
      AIMRT_WARN("Can not find service func '{}' in pkg '{}' module '{}'",
                 client_info.func_name, service_pkg_path, service_module_name);

      client_invoke_wrapper_ptr->callback(aimrt::rpc::Status(AIMRT_RPC_STATUS_CLI_BACKEND_INTERNAL_ERROR));
      return;
    }

    // 创建服务调用包装器
    auto service_invoke_wrapper_ptr = std::make_shared<InvokeWrapper>(InvokeWrapper{
        .info = service_func_wrapper_ptr->info});
    const auto& service_info = service_invoke_wrapper_ptr->info;

    // 创建服务上下文
    auto ctx_ptr = std::make_shared<aimrt::rpc::Context>(aimrt_rpc_context_type_t::AIMRT_RPC_SERVER_CONTEXT);
    service_invoke_wrapper_ptr->ctx_ref = ctx_ptr;

    ctx_ptr->SetTimeout(client_invoke_wrapper_ptr->ctx_ref.Timeout());

    const auto& meta_keys = client_invoke_wrapper_ptr->ctx_ref.GetMetaKeys();
    for (const auto& item : meta_keys)
      ctx_ptr->SetMetaValue(item, client_invoke_wrapper_ptr->ctx_ref.GetMetaValue(item));

    ctx_ptr->SetFunctionName(service_info.func_name);
    ctx_ptr->SetMetaValue(AIMRT_RPC_CONTEXT_KEY_BACKEND, Name());
    ctx_ptr->SetMetaValue("aimrt-from_pkg", client_info.pkg_path);
    ctx_ptr->SetMetaValue("aimrt-from_module", client_info.module_name);

    // 在同一个包内调用时，可以直接传递指针而无需序列化
    if (service_pkg_path == client_info.pkg_path) {
      service_invoke_wrapper_ptr->req_ptr = client_invoke_wrapper_ptr->req_ptr;
      service_invoke_wrapper_ptr->rsp_ptr = client_invoke_wrapper_ptr->rsp_ptr;
      service_invoke_wrapper_ptr->callback =
          [ctx_ptr,
           service_invoke_wrapper_ptr,
           client_invoke_wrapper_ptr](aimrt::rpc::Status status) {
            client_invoke_wrapper_ptr->callback(status);
          };

      service_func_wrapper_ptr->service_func(service_invoke_wrapper_ptr);
      return;
    }

    // 跨包调用时，需要进行序列化/反序列化，并启用超时功能

    // 记录请求信息，用于超时处理和回调
    auto timeout = client_invoke_wrapper_ptr->ctx_ref.Timeout();
    uint32_t cur_req_id = req_id_++;
    auto record_ptr = client_invoke_wrapper_ptr;

    bool ret = client_tool_ptr_->Record(cur_req_id, timeout, std::move(record_ptr));

    if (omnirt_unlikely(!ret)) {
      // 记录请求失败，通常不应该发生
      AIMRT_ERROR("Failed to record msg.");
      client_invoke_wrapper_ptr->callback(aimrt::rpc::Status(AIMRT_RPC_STATUS_CLI_BACKEND_INTERNAL_ERROR));
      return;
    }

    // 对客户端请求进行序列化
    std::string serialization_type(client_invoke_wrapper_ptr->ctx_ref.GetSerializationType());

    auto buffer_array_view_ptr = TrySerializeReqWithCache(*client_invoke_wrapper_ptr, serialization_type);
    if (omnirt_unlikely(!buffer_array_view_ptr)) {
      // 序列化失败
      AIMRT_ERROR(
          "Req serialization failed in local rpc backend, serialization_type {}, pkg_path: {}, module_name: {}, func_name: {}",
          serialization_type, client_info.pkg_path, client_info.module_name, client_info.func_name);

      client_invoke_wrapper_ptr->callback(aimrt::rpc::Status(AIMRT_RPC_STATUS_CLI_SERIALIZATION_FAILED));
      return;
    }

    // 将序列化的请求数据反序列化为服务端请求对象
    std::shared_ptr<void> service_req_ptr = service_info.req_type_support_ref.CreateSharedPtr();
    service_invoke_wrapper_ptr->req_ptr = service_req_ptr.get();

    bool deserialize_ret = service_info.req_type_support_ref.Deserialize(
        serialization_type,
        *(buffer_array_view_ptr->NativeHandle()),
        service_req_ptr.get());

    if (omnirt_unlikely(!deserialize_ret)) {
      // 反序列化失败
      AIMRT_FATAL(
          "Rsp deserialization failed in local rpc backend, serialization_type {}, pkg_path: {}, module_name: {}, func_name: {}",
          serialization_type, service_info.pkg_path, service_info.module_name, service_info.func_name);

      client_invoke_wrapper_ptr->callback(aimrt::rpc::Status(AIMRT_RPC_STATUS_CLI_DESERIALIZATION_FAILED));
      return;
    }

    // 缓存服务请求反序列化使用的缓冲区，避免重复创建
    service_invoke_wrapper_ptr->req_serialization_cache.emplace(serialization_type, buffer_array_view_ptr);

    // 创建服务响应对象
    std::shared_ptr<void> service_rsp_ptr = service_info.rsp_type_support_ref.CreateSharedPtr();
    service_invoke_wrapper_ptr->rsp_ptr = service_rsp_ptr.get();

    // 设置服务调用完成后的回调函数
    service_invoke_wrapper_ptr->callback =
        [this,
         service_invoke_wrapper_ptr,
         ctx_ptr,
         cur_req_id,
         service_req_ptr,
         service_rsp_ptr,
         serialization_type{std::move(serialization_type)}](aimrt::rpc::Status status) {
          auto msg_recorder = client_tool_ptr_->GetRecord(cur_req_id);
          if (omnirt_unlikely(!msg_recorder)) {
            // 未找到请求记录，表明该请求已超时并被超时处理程序移除
            AIMRT_TRACE("Can not get req id {} from recorder.", cur_req_id);
            return;
          }

          // 成功获取到请求记录，继续处理响应
          auto client_invoke_wrapper_ptr = std::move(*msg_recorder);
          const auto& client_info = client_invoke_wrapper_ptr->info;

          const auto& service_info = service_invoke_wrapper_ptr->info;

          // 对服务响应进行序列化
          auto buffer_array_view_ptr = TrySerializeRspWithCache(*service_invoke_wrapper_ptr, serialization_type);
          if (omnirt_unlikely(!buffer_array_view_ptr)) {
            // 服务响应序列化失败，返回错误状态
            AIMRT_ERROR(
                "Rsp serialization failed in local rpc backend, serialization_type {}, pkg_path: {}, module_name: {}, func_name: {}",
                serialization_type, service_info.pkg_path, service_info.module_name, service_info.func_name);

            client_invoke_wrapper_ptr->callback(aimrt::rpc::Status(AIMRT_RPC_STATUS_SVR_SERIALIZATION_FAILED));

            return;
          }

          // 将序列化的响应数据反序列化为客户端响应对象
          bool deserialize_ret = client_info.rsp_type_support_ref.Deserialize(
              serialization_type,
              *(buffer_array_view_ptr->NativeHandle()),
              client_invoke_wrapper_ptr->rsp_ptr);

          if (!deserialize_ret) {
            // 客户端响应反序列化失败，返回错误状态
            AIMRT_ERROR(
                "Req deserialization failed in local rpc backend, serialization_type {}, pkg_path: {}, module_name: {}, func_name: {}",
                serialization_type, client_info.pkg_path, client_info.module_name, client_info.func_name);

            client_invoke_wrapper_ptr->callback(aimrt::rpc::Status(AIMRT_RPC_STATUS_SVR_DESERIALIZATION_FAILED));

            return;
          }

          // 缓存客户端响应反序列化使用的缓冲区
          client_invoke_wrapper_ptr->rsp_serialization_cache.emplace(serialization_type, buffer_array_view_ptr);

          // 调用客户端回调函数，完成RPC调用流程
          client_invoke_wrapper_ptr->callback(status);
        };

    // 执行服务RPC函数调用
    service_func_wrapper_ptr->service_func(service_invoke_wrapper_ptr);
  } catch (const std::exception& e) {
    AIMRT_ERROR("{}", e.what());
  }
}

/**
 * @brief 注册获取执行器的函数
 * @param get_executor_func 根据执行器名称获取执行器的函数
 * @throw 当状态不是预初始化状态时抛出异常
 * @details 设置用于获取超时执行器的函数，必须在初始化前调用
 */
void LocalRpcBackend::RegisterGetExecutorFunc(
    const std::function<aimrt::executor::ExecutorRef(std::string_view)>& get_executor_func) {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kPreInit,
      "Method can only be called when state is 'PreInit'.");
  get_executor_func_ = get_executor_func;
}

}  // namespace aimrt::runtime::core::rpc
