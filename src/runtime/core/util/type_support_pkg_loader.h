// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <vector>

#include "aimrt_module_c_interface/util/type_support_base.h"

#include "core/util/dynamic_lib.h"
#include "util/log_util.h"

namespace aimrt::runtime::core::util {

class TypeSupportPkgLoader {
 public:
  TypeSupportPkgLoader() = default;
  ~TypeSupportPkgLoader() { UnLoadTypeSupportPkg(); }

  TypeSupportPkgLoader(const TypeSupportPkgLoader&) = delete;
  TypeSupportPkgLoader& operator=(const TypeSupportPkgLoader&) = delete;

  void LoadTypeSupportPkg(std::string_view path);

  void UnLoadTypeSupportPkg();

  // 返回类型支持数组的视图
  const aimrt_type_support_base_t* const* GetTypeSupportArray() const { 
    return type_support_array_.data(); 
  }

  size_t GetTypeSupportArraySize() const {
    return type_support_array_.size();
  }

  auto& GetDynamicLib() { return dynamic_lib_; }

 private:
  std::string path_;
  aimrt::common::util::DynamicLib dynamic_lib_;

  // 使用vector替代span
  std::vector<const aimrt_type_support_base_t*> type_support_array_;
};

}  // namespace aimrt::runtime::core::util
