// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include "core/parameter/parameter_handle.h"
#include "util/macros.h"

namespace aimrt::runtime::core::parameter {

std::shared_ptr<const std::string> ParameterHandle::GetParameter(std::string_view key) {
  std::lock_guard<std::mutex> lck(parameter_map_mutex_);

  auto find_itr = parameter_map_.find(key);
  if (find_itr != parameter_map_.end()) {
    AIMRT_TRACE("Get parameter '{}'", key);

    return find_itr->second;
  }

  AIMRT_TRACE("Can not get parameter '{}'", key);
  return std::shared_ptr<std::string>();
}

// 获取参数值（模板函数自动类型推导）
template <typename T>
std::optional<T> ParameterHandle::GetParameter(const std::string& key) const {

}

void ParameterHandle::SetParameter(
    std::string_view key, const std::shared_ptr<std::string>& value_ptr) {
  std::lock_guard<std::mutex> lck(parameter_map_mutex_);

  auto find_itr = parameter_map_.find(key);
  if (find_itr != parameter_map_.end()) {
    if (omnirt_unlikely(!value_ptr || value_ptr->empty())) {
      AIMRT_TRACE("Erase parameter '{}'", key);
      parameter_map_.erase(find_itr);
      return;
    }

    AIMRT_TRACE("Update parameter '{}'", key);
    find_itr->second = value_ptr;

    return;
  }

  AIMRT_TRACE("Set parameter '{}'", key);
  parameter_map_.emplace(key, value_ptr);
}

// 设置参数值（支持任意可序列化类型）
template <typename T>
void ParameterHandle::SetParameter(std::string_view key, const T& value) {
 
}

std::vector<std::string> ParameterHandle::ListParameter() const {
  std::lock_guard<std::mutex> lck(parameter_map_mutex_);

  std::vector<std::string> result;
  result.reserve(parameter_map_.size());

  for (const auto& itr : parameter_map_) {
    result.emplace_back(itr.first);
  }

  return result;
}

}  // namespace aimrt::runtime::core::parameter
