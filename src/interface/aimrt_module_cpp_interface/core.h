// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include "util/exception.h"
#include "aimrt_module_cpp_interface/util/string.h"
#include "aimrt_module_c_interface/core_base.h"

#if ENABLE_ALLOCATOR
#include "aimrt_module_cpp_interface/allocator/allocator.h"
#endif

#if ENABLE_CONFIGURATOR
#include "aimrt_module_cpp_interface/configurator/configurator.h"
#endif

#if ENABLE_EXECUTOR
#include "aimrt_module_cpp_interface/executor/executor_manager.h"
#endif

#if ENABLE_LOGGER
#include "aimrt_module_cpp_interface/logger/logger.h"
#endif

#if ENABLE_CHANNEL
#include "aimrt_module_cpp_interface/channel/channel_handle.h"
#endif

#if ENABLE_RPC
#include "aimrt_module_cpp_interface/rpc/rpc_handle.h"
#endif

#if ENABLE_PARAMETER
#include "aimrt_module_cpp_interface/parameter/parameter_handle.h"
#endif

#if ENABLE_LLM
#include "aimrt_module_cpp_interface/llm/llm_handle.h"
#endif

#if ENABLE_MODULE_MANAGER
#include "aimrt_module_cpp_interface/module/module_manager.h"
#endif


namespace aimrt {

struct ModuleInfo {
  std::string_view name;

  uint32_t major_version = 0;
  uint32_t minor_version = 0;
  uint32_t patch_version = 0;
  uint32_t build_version = 0;

  std::string_view author;
  std::string_view description;
};

/**
 * @brief Abstract of framework runtime, providing some functions for modules
 *
 */
class CoreRef {
 public:
  CoreRef() = default;
  explicit CoreRef(const aimrt_core_base_t* base_ptr)
      : base_ptr_(base_ptr) {}
  ~CoreRef() = default;

  explicit operator bool() const { return (base_ptr_ != nullptr); }

  const aimrt_core_base_t* NativeHandle() const { return base_ptr_; }

  ModuleInfo Info() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    aimrt_module_info_t module_info = base_ptr_->info(base_ptr_->impl);
    return ModuleInfo{
        .name = util::ToStdStringView(module_info.name),
        .major_version = module_info.major_version,
        .minor_version = module_info.minor_version,
        .patch_version = module_info.patch_version,
        .build_version = module_info.build_version,
        .author = util::ToStdStringView(module_info.author),
        .description = util::ToStdStringView(module_info.description)};
  }

  /**
   * @brief Get configurator handle
   *
   * @return ConfiguratorRef
   */
#if ENABLE_CONFIGURATOR
  configurator::ConfiguratorRef GetConfigurator() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return configurator::ConfiguratorRef(base_ptr_->configurator(base_ptr_->impl));
  }
#endif

  /**
   * @brief Get allocator handle
   *
   * @return allocator::AllocatorRef
   */
#if ENABLE_ALLOCATOR
  allocator::AllocatorRef GetAllocator() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return allocator::AllocatorRef(base_ptr_->allocator_handle(base_ptr_->impl));
  }
#endif

  /**
   * @brief Get executor manager handle
   *
   * @return ExecutorManagerRef
   */
#if ENABLE_EXECUTOR
  executor::ExecutorManagerRef GetExecutorManager() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return executor::ExecutorManagerRef(base_ptr_->executor_manager(base_ptr_->impl));
  }
#endif

  /**
   * @brief Get logger handle
   *
   * @return logger::LoggerRef
   */
#if ENABLE_LOGGER
  logger::LoggerRef GetLogger() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return logger::LoggerRef(base_ptr_->logger(base_ptr_->impl));
  }
#endif

  /**
   * @brief Get rpc handle
   *
   * @return rpc::RpcHandleRef
   */
#if ENABLE_RPC
  rpc::RpcHandleRef GetRpcHandle() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return rpc::RpcHandleRef(base_ptr_->rpc_handle(base_ptr_->impl));
  }
#endif

  /**
   * @brief Get channel handle
   *
   * @return channel::ChannelHandleRef
   */
#if ENABLE_CHANNEL
  channel::ChannelHandleRef GetChannelHandle() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return channel::ChannelHandleRef(base_ptr_->channel_handle(base_ptr_->impl));
  }
#endif

  /**
   * @brief Get parameter handle
   *
   * @return parameter::ParameterHandleRef
   */
#if ENABLE_PARAMETER
  parameter::ParameterHandleRef GetParameterHandle() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return parameter::ParameterHandleRef(base_ptr_->parameter_handle(base_ptr_->impl));
  }
#endif

#if ENABLE_LLM
  llm::LlmHandleRef GetLlmHandle() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return llm::LlmHandleRef(base_ptr_->llm_handle(base_ptr_->impl));
  }
#endif

#if ENABLE_MODULE_MANAGER
  module::ModuleManagerRef GetModuleManager() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return module::ModuleManagerRef(base_ptr_->module_manager(base_ptr_->impl));
  }
#endif

 private:
  const aimrt_core_base_t* base_ptr_ = nullptr;
};

}  // namespace aimrt
