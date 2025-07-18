// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#ifndef AIMRT_VERSION
#define AIMRT_VERSION "1.0.0"
#endif

namespace aimrt::runtime::core::util {

inline const char* GetAimRTVersion() {
  return AIMRT_VERSION;
}
}  // namespace aimrt::runtime::core::util
