#pragma once
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <any>
#include "manager_base.h"

namespace aimrt::runtime::core {

class ManagerContainer {
 public:
  template<typename T>
  void RegisterService(std::shared_ptr<T> service) {
    services_[std::type_index(typeid(T))] = service;
  }
  
  template<typename T>
  std::shared_ptr<T> GetService() const {
    auto it = services_.find(std::type_index(typeid(T)));
    if (it != services_.end()) {
      return std::any_cast<std::shared_ptr<T>>(it->second);
    }
    return nullptr;
  }
  
 private:
  std::unordered_map<std::type_index, std::any> services_;
};

} // namespace aimrt::runtime::core
