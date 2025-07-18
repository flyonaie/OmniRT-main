// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <string>
#include <vector>
#include "aimrt_module_c_interface/module_manager_base.h"
#include "aimrt_module_cpp_interface/util/string.h"


namespace aimrt::module {

class ModuleManagerRef {
 public:
  ModuleManagerRef() = default;
  explicit ModuleManagerRef(const aimrt_module_manager_base_t* base_ptr)
      : base_ptr_(base_ptr) {}
  ~ModuleManagerRef() = default;

  explicit operator bool() const { return (base_ptr_ != nullptr); }

  const aimrt_module_manager_base_t* NativeHandle() const {
    return base_ptr_;
  }

  std::vector<std::string> GetModuleList() const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return base_ptr_->get_module_list(base_ptr_->impl);
  }

  void* GetModule(const std::string& module_name) const {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    return base_ptr_->get_module(base_ptr_->impl, module_name);
  }

 private:
  const aimrt_module_manager_base_t* base_ptr_ = nullptr;
};

}  // namespace aimrt::module