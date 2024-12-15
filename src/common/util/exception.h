// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <string>

#include "util/format.h"

namespace aimrt::common::util {

/**
 * @brief 异常处理基类
 * 
 * 提供了一个统一的异常处理机制，所有自定义异常类都继承自此类。
 * 支持通过可变参数模板构造格式化的错误消息，方便记录详细的错误信息。
 * 
 * 用法示例:
 * @code
 *   throw AimRTException("Failed to open file: {}", filename);
 *   throw AimRTException("Invalid value {} for parameter {}", value, param);
 * @endcode
 */
class AimRTException : public std::exception {
 public:
  /**
   * @brief 构造函数
   * 
   * 通过可变参数模板和完美转发支持格式化的错误消息构造。
   * 使用static_assert确保参数可以构造std::string。
   * 
   * @tparam Args 可变参数模板类型
   * @param args 用于构造错误消息的参数
   */
  template <typename... Args>
    // requires std::constructible_from<std::string, Args...>
  AimRTException(Args... args)
      : err_msg_(std::forward<Args>(args)...) {
        static_assert(std::is_constructible<std::string, Args...>::value, "Args must be constructible to std::string");
      }

  ~AimRTException() noexcept override {}

  /**
   * @brief 获取错误消息
   * 
   * 重写std::exception::what()函数，返回C风格字符串。
   * 这样设计可以保持与标准库异常处理机制的兼容性。
   * 
   * @return const char* 错误消息的C风格字符串
   */
  const char* what() const noexcept override { return err_msg_.c_str(); }

 private:
  std::string err_msg_;  ///< 存储格式化后的错误消息
};

}  // namespace aimrt::common::util

/**
 * @brief 断言宏
 * 
 * 用于运行时条件检查，当表达式为false时抛出异常。
 * 支持格式化的错误消息，方便提供详细的错误信息。
 * 使用[[unlikely]]提示编译器优化。
 * 
 * 用法示例:
 * @code
 *   AIMRT_ASSERT(value > 0, "Value must be positive, got {}", value);
 *   AIMRT_ASSERT(file != nullptr, "Failed to open file: {}", filename);
 * @endcode
 * 
 * @param __expr__ 要检查的表达式
 * @param __fmt__ 错误消息格式字符串
 * @param ... 格式化参数
 */
#define AIMRT_ASSERT(__expr__, __fmt__, ...)                                                  \
  do {                                                                                        \
    [[unlikely]] if (!(__expr__)) {                                                           \
      throw aimrt::common::util::AimRTException(::aimrt_fmt::format(__fmt__, ##__VA_ARGS__)); \
    }                                                                                         \
  } while (0)
