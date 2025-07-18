// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <memory>
#include <string>
#include <functional>

#include "aimrt_module_c_interface/module_manager_base.h"
#include "util/string_util.h"

namespace aimrt::runtime::core::module {

class ModuleManagerCallbacks {
 public:
  std::function<std::vector<std::string>()> get_module_list;
  std::function<void*(const std::string& module_name)> get_module;
  // 未来可以添加更多回调...
};

class ModuleManagerProxy {
 public:
  explicit ModuleManagerProxy(ModuleManagerCallbacks& callbacks)
      : callbacks_(callbacks),
        base_(GenBase(this)) {}

  ~ModuleManagerProxy() = default;

  ModuleManagerProxy(const ModuleManagerProxy&) = delete;
  ModuleManagerProxy& operator=(const ModuleManagerProxy&) = delete;

  const aimrt_module_manager_base_t* NativeHandle() const { return &base_; }

 private:
  std::vector<std::string> GetModuleList() const {   
    return callbacks_.get_module_list();
  }

  void* GetModule(const std::string& module_name) const {   
    return callbacks_.get_module(module_name);
  }

  static aimrt_module_manager_base_t GenBase(void* impl) {
    return aimrt_module_manager_base_t{
        .get_module_list = [](void* impl) -> std::vector<std::string> {
          return static_cast<ModuleManagerProxy*>(impl)->GetModuleList();
        },
        .get_module = [](void* impl, const std::string& module_name) -> void* {
          return static_cast<ModuleManagerProxy*>(impl)->GetModule(module_name);
        },
        .impl = impl};
  }

 private:
  ModuleManagerCallbacks& callbacks_;
  const aimrt_module_manager_base_t base_;
};

}  // namespace aimrt::runtime::core::module