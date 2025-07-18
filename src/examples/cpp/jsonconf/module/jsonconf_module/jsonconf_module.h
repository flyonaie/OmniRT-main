// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <atomic>

#include "aimrt_module_cpp_interface/module_base.h"

namespace aimrt::examples::cpp::jsonconf::jsonconf_module {

class JsonconfModule : public aimrt::ModuleBase {
 public:
  JsonconfModule() = default;
  ~JsonconfModule() override = default;

  ModuleInfo Info() const override {
    return ModuleInfo{.name = "JsonconfModule"};
  }

  bool Initialize(aimrt::CoreRef core) override;

  bool Start() override;

  void Shutdown() override;

 private:
  auto GetLogger() { return core_.GetLogger(); }

 private:
  aimrt::CoreRef core_;
};

}  // namespace aimrt::examples::cpp::jsonconf::jsonconf_module
