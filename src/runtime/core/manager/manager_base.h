#pragma once
#include <memory>
#include <string>
#include <string_view>

namespace aimrt::runtime::core {

// 管理器状态枚举
enum class ManagerState {
  kUninitialized,
  kInitialized,
  kRunning,
  kStopped,
  kError
};

// 管理器配置基类
struct ManagerConfig {
  virtual ~ManagerConfig() = default;
};

class ManagerBase {
 public:
  virtual ~ManagerBase() = default;
  
  // 核心生命周期方法
  virtual void Initialize(const ManagerConfig* config) = 0;
  virtual void Start() = 0;
  virtual void Stop() = 0;
  
  // 状态查询
  ManagerState GetState() const noexcept { return state_; }
  const std::string& GetError() const noexcept { return error_; }
  bool IsHealthy() const noexcept { return state_ != ManagerState::kError; }
  
  // 管理器标识
  virtual std::string_view GetName() const = 0;

 protected:
  void SetState(ManagerState state) noexcept { state_ = state; }
  void SetError(std::string error) noexcept { 
    error_ = std::move(error);
    state_ = ManagerState::kError;
  }

 private:
  ManagerState state_ = ManagerState::kUninitialized;
  std::string error_;
};

using ManagerPtr = std::shared_ptr<ManagerBase>;

} // namespace aimrt::runtime::core
