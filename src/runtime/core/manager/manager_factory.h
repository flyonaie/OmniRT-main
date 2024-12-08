#pragma once
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>
#include "manager_base.h"

namespace aimrt::runtime::core {

class ManagerFactory {
 public:
  using Creator = std::function<ManagerPtr()>;
  
  static ManagerFactory& Instance() {
    static ManagerFactory instance;
    return instance;
  }

  template<typename T>
  void RegisterManager(std::string_view name) {
    static_assert(std::is_base_of_v<ManagerBase, T>,
                 "Manager must inherit from ManagerBase");
                 
    std::lock_guard<std::mutex> lock(mutex_);
    creators_[std::string(name)] = []() { return std::make_shared<T>(); };
  }

  ManagerPtr CreateManager(std::string_view name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = creators_.find(std::string(name));
    if (it != creators_.end()) {
      return it->second();
    }
    return nullptr;
  }
  
  std::vector<std::string> GetRegisteredManagers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(creators_.size());
    for (const auto& [name, _] : creators_) {
      names.push_back(name);
    }
    return names;
  }

 private:
  ManagerFactory() = default;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, Creator> creators_;
};

// 简化的注册宏
#define REGISTER_MANAGER(name, type) \
  namespace { \
    struct name##_registrar { \
      name##_registrar() { \
        ManagerFactory::Instance().RegisterManager<type>(#name); \
      } \
    }; \
    static name##_registrar name##_instance; \
  }

} // namespace aimrt::runtime::core
