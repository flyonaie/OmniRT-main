// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.
//
// 字符串格式化工具
// 提供统一的字符串格式化接口，支持两种实现方式：
// 1. fmt库: 高性能、功能丰富的第三方格式化库
// 2. std::format: C++20标准库提供的格式化功能
//
// 特性说明:
// - 编译时格式字符串检查
// - 类型安全的参数替换
// - 国际化支持(Unicode)
// - 自定义类型格式化扩展
//
// 配置说明:
// - 通过宏AIMRT_USE_FMT_LIB控制使用哪个格式化库:
//   * 定义: 使用fmt库
//   * 未定义: 使用std::format
//
// 性能考虑:
// - fmt库通常比std::format有更好的性能
// - fmt库支持编译期格式化，减少运行时开销
// - 支持零内存分配的格式化操作
//
// 使用示例:
// @code
//   #include "format.h"
//   std::string s = aimrt_fmt::format("Hello, {}!", "world");
//   int value = 42;
//   std::string num = aimrt_fmt::format("Value: {:d}", value);
// @endcode

#pragma once

#ifdef AIMRT_USE_FMT_LIB
  /**
   * @brief 使用fmt库进行格式化
   * 
   * fmt库特性:
   * - 更快的格式化性能
   * - 更小的二进制体积
   * - 更丰富的格式化选项
   * - 支持chrono时间类型
   */
  #include "fmt/chrono.h"  // 时间类型格式化支持
  #include "fmt/core.h"    // 核心格式化功能
  #include "fmt/format.h"  // 扩展格式化功能

  #define aimrt_fmt fmt

#else
  /**
   * @brief 使用C++标准库的std::format
   * 
   * std::format特性:
   * - C++20标准库组件
   * - 无需额外依赖
   * - 与标准库其他组件良好集成
   * - 需要编译器支持C++20
   */
  #include <format>

  #define aimrt_fmt std
#endif
