// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "aimrt_module_cpp_interface/executor/executor.h"
#include "core/rpc/rpc_backend_manager.h"
#include "core/rpc/rpc_handle_proxy.h"
#include "core/util/module_detail_info.h"
#include "util/log_util.h"

namespace aimrt::runtime::core::rpc {

/**
 * @brief RPC管理器类
 * 
 * 该类负责管理RPC(远程过程调用)相关的所有功能,包括:
 * - RPC后端的注册和管理
 * - RPC客户端和服务端的配置
 * - RPC过滤器的注册
 * - RPC句柄代理的管理
 */
class RpcManager {
 public:
  /**
   * @brief RPC管理器配置选项
   */
  struct Options {
    /**
     * @brief 后端配置选项
     */
    struct BackendOptions {
      std::string type;        ///< 后端类型
      YAML::Node options;      ///< 后端具体配置
    };
    std::vector<BackendOptions> backends_options;  ///< 所有后端的配置列表

    /**
     * @brief 客户端配置选项
     */
    struct ClientOptions {
      std::string func_name;                    ///< 函数名称
      std::vector<std::string> enable_backends; ///< 启用的后端列表
      std::vector<std::string> enable_filters;  ///< 启用的过滤器列表
    };
    std::vector<ClientOptions> clients_options;  ///< 所有客户端的配置列表

    /**
     * @brief 服务端配置选项
     */
    struct ServerOptions {
      std::string func_name;                    ///< 函数名称
      std::vector<std::string> enable_backends; ///< 启用的后端列表
      std::vector<std::string> enable_filters;  ///< 启用的过滤器列表
    };
    std::vector<ServerOptions> servers_options;  ///< 所有服务端的配置列表
  };

  /**
   * @brief RPC管理器状态枚举
   */
  enum class State : uint32_t {
    kPreInit,   ///< 预初始化状态
    kInit,      ///< 初始化完成状态
    kStart,     ///< 启动运行状态
    kShutdown,  ///< 已关闭状态
  };

 public:
  /**
   * @brief 构造函数
   */
  RpcManager()
      : logger_ptr_(std::make_shared<aimrt::common::util::LoggerWrapper>()) {}
  
  /**
   * @brief 析构函数
   */
  ~RpcManager() = default;

  RpcManager(const RpcManager&) = delete;
  RpcManager& operator=(const RpcManager&) = delete;

  /**
   * @brief 初始化RPC管理器
   * @param options_node YAML格式的配置节点
   */
  void Initialize(YAML::Node options_node);

  /**
   * @brief 启动RPC管理器
   */
  void Start();

  /**
   * @brief 关闭RPC管理器
   */
  void Shutdown();

  /**
   * @brief 获取配置选项
   * @return 当前的配置选项
   */
  const Options& GetOptions() const { return options_; }

  /**
   * @brief 获取当前状态
   * @return 当前的运行状态
   */
  State GetState() const { return state_.load(); }

  /**
   * @brief 设置日志器
   * @param logger_ptr 日志器指针
   */
  void SetLogger(const std::shared_ptr<aimrt::common::util::LoggerWrapper>& logger_ptr) { logger_ptr_ = logger_ptr; }

  /**
   * @brief 获取日志器
   * @return 当前的日志器引用
   */
  const aimrt::common::util::LoggerWrapper& GetLogger() const { return *logger_ptr_; }

  /**
   * @brief 生成初始化报告
   * @return 包含初始化信息的键值对列表
   */
  std::list<std::pair<std::string, std::string>> GenInitializationReport() const;

  /**
   * @brief 注册RPC后端
   * @param rpc_backend_ptr RPC后端指针
   */
  void RegisterRpcBackend(std::unique_ptr<RpcBackendBase>&& rpc_backend_ptr);

  /**
   * @brief 注册获取执行器的函数
   * @param get_executor_func 获取执行器的函数对象
   */
  void RegisterGetExecutorFunc(
      const std::function<aimrt::executor::ExecutorRef(std::string_view)>& get_executor_func);

  /**
   * @brief 获取RPC句柄代理
   * @param module_info 模块详细信息
   * @return RPC句柄代理的常量引用
   */
  const RpcHandleProxy& GetRpcHandleProxy(const util::ModuleDetailInfo& module_info);

  /**
   * @brief 获取RPC句柄代理(重载版本)
   * @param module_name 模块名称
   * @return RPC句柄代理的常量引用
   */
  const RpcHandleProxy& GetRpcHandleProxy(std::string_view module_name = "core") {
    return GetRpcHandleProxy(util::ModuleDetailInfo{.name = std::string(module_name), .pkg_path = "core"});
  }

  /**
   * @brief 注册客户端过滤器
   * @param name 过滤器名称
   * @param filter 过滤器函数对象
   */
  void RegisterClientFilter(std::string_view name, FrameworkAsyncRpcFilter&& filter);

  /**
   * @brief 注册服务端过滤器
   * @param name 过滤器名称
   * @param filter 过滤器函数对象
   */
  void RegisterServerFilter(std::string_view name, FrameworkAsyncRpcFilter&& filter);

  /**
   * @brief 添加传递的上下文元数据键
   * @param keys 键集合
   */
  void AddPassedContextMetaKeys(const std::unordered_set<std::string>& keys);

  /**
   * @brief 获取RPC注册表
   * @return RPC注册表指针
   */
  const RpcRegistry* GetRpcRegistry() const;

  /**
   * @brief 获取已使用的RPC后端列表
   * @return RPC后端指针列表
   */
  const std::vector<RpcBackendBase*>& GetUsedRpcBackend() const;

 private:
  /**
   * @brief 注册本地RPC后端
   */
  void RegisterLocalRpcBackend();

  /**
   * @brief 注册调试日志过滤器
   */
  void RegisterDebugLogFilter();

 private:
  Options options_;                                    ///< 配置选项
  std::atomic<State> state_ = State::kPreInit;        ///< 当前状态
  std::shared_ptr<aimrt::common::util::LoggerWrapper> logger_ptr_;  ///< 日志器指针

  std::function<aimrt::executor::ExecutorRef(std::string_view)> get_executor_func_;  ///< 获取执行器的函数

  std::unordered_set<std::string> passed_context_meta_keys_;  ///< 传递的上下文元数据键集合

  FrameworkAsyncRpcFilterManager client_filter_manager_;  ///< 客户端过滤器管理器
  FrameworkAsyncRpcFilterManager server_filter_manager_;  ///< 服务端过滤器管理器

  std::unique_ptr<RpcRegistry> rpc_registry_ptr_;        ///< RPC注册表指针

  std::vector<std::unique_ptr<RpcBackendBase>> rpc_backend_vec_;      ///< RPC后端列表
  std::vector<RpcBackendBase*> used_rpc_backend_vec_;                 ///< 使用中的RPC后端列表

  RpcBackendManager rpc_backend_manager_;                             ///< RPC后端管理器

  std::unordered_map<std::string, std::unique_ptr<RpcHandleProxy>> rpc_handle_proxy_map_;  ///< RPC句柄代理映射表
};

}  // namespace aimrt::runtime::core::rpc
