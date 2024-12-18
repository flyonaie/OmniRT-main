// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.
//
// 日志工具库
// 提供灵活的日志记录功能，支持同步和异步日志记录，以及不同的日志级别。
//
// 主要特性:
// 1. 日志级别控制
//    - 支持TRACE/DEBUG/INFO/WARN/ERROR/FATAL六个级别
//    - 可动态调整日志级别
// 2. 日志格式化
//    - 支持fmt库格式化
//    - 自动包含时间戳、线程ID、源文件信息等
// 3. 异步日志
//    - 提供异步日志记录功能
//    - 使用阻塞队列实现
// 4. 跨平台支持
//    - 支持Linux和Windows
//    - 自动适配不同平台的线程ID获取方式
//
// 使用示例:
// @code
//   // 基本日志记录
//   AIMRT_INFO("Hello %s", "world");
//   
//   // 条件检查和日志
//   AIMRT_CHECK_ERROR(ptr != nullptr, "Pointer is null");
//   
//   // 异步日志记录
//   SimpleAsyncLogger logger;
//   AIMRT_HL_INFO(logger, "Async log message");
// @endcode

#pragma once

#include <functional>

#if __cplusplus < 202002L
#include <string>
#include <cstdint>

/**
 * @brief 源代码位置信息兼容层
 * 
 * 在C++20之前的版本中模拟std::source_location功能
 * 提供文件名、行号、函数名等源码位置信息
 */
#define CURRENT_FILE __FILE__
#define CURRENT_LINE __LINE__
#define CURRENT_FUNCTION __FUNCTION__
class src_loc {
 public:
    struct source_location {
      static constexpr source_location current() noexcept {
          return source_location{};
      }
      constexpr const char* file_name() const noexcept { return __FILE__; }
      constexpr std::uint_least32_t line() const noexcept { return __LINE__; }
      constexpr const char* function_name() const noexcept { return __FUNCTION__; }
      constexpr std::uint_least32_t column() const noexcept { return 0; }
  };
};
#define aimrt_src_loc src_loc
#else
#include <source_location>
#define aimrt_src_loc std
#endif

#include <string>
#include <thread>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "util/block_queue.h"
#include "util/exception.h"
#include "util/format.h"
#include "util/time_util.h"

#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
  #define gettid() syscall(SYS_gettid)
#else
  #define gettid() ::gettid()
#endif

namespace aimrt::common::util {

/**
 * @brief 日志级别常量定义
 * 
 * 定义六个标准日志级别，从低到高分别是:
 * - TRACE: 最详细的跟踪信息
 * - DEBUG: 调试信息
 * - INFO:  一般信息
 * - WARN:  警告信息
 * - ERROR: 错误信息
 * - FATAL: 致命错误
 */
constexpr uint32_t kLogLevelTrace = 0;
constexpr uint32_t kLogLevelDebug = 1;
constexpr uint32_t kLogLevelInfo = 2;
constexpr uint32_t kLogLevelWarn = 3;
constexpr uint32_t kLogLevelError = 4;
constexpr uint32_t kLogLevelFatal = 5;

/**
 * @brief 简单同步日志记录器
 * 
 * 提供基本的同步日志记录功能:
 * - 支持不同日志级别
 * - 自动记录时间戳和线程ID
 * - 包含源代码位置信息
 * 
 * 示例:
 * @code
 *   SimpleLogger::Log(kLogLevelInfo, __LINE__, 0, __FILE__,
 *                     __FUNCTION__, "Test message", 12);
 * @endcode
 */
class SimpleLogger {
 public:
  /**
   * @brief 获取当前日志级别
   * @return 始终返回0，允许所有级别的日志
   */
  static uint32_t GetLogLevel() { return 0; }

  /**
   * @brief 记录一条日志
   * 
   * @param lvl 日志级别
   * @param line 源代码行号
   * @param column 源代码列号
   * @param file_name 源文件名
   * @param function_name 函数名
   * @param log_data 日志内容
   * @param log_data_size 日志内容长度
   * 
   * 日志格式:
   * [时间.微秒][级别][线程ID][文件:行:列 @函数]消息
   */
  static void Log(uint32_t lvl,
                  uint32_t line,
                  uint32_t column,
                  const char* file_name,
                  const char* function_name,
                  const char* log_data,
                  size_t log_data_size) {
    static constexpr std::string_view kLvlNameArray[] = {
        "Trace", "Debug", "Info", "Warn", "Error", "Fatal"};

    static constexpr uint32_t kLvlNameArraySize =
        sizeof(kLvlNameArray) / sizeof(kLvlNameArray[0]);
    [[unlikely]] if (lvl >= kLvlNameArraySize) 
      lvl = kLvlNameArraySize - 1;

#if defined(_WIN32)
    thread_local size_t tid(std::hash<std::thread::id>{}(std::this_thread::get_id()));
#else
    thread_local size_t tid(gettid());
#endif

    auto t = std::chrono::system_clock::now();
    std::string log_str = ::aimrt_fmt::format(
        "[{}.{:0>6}][{}][{}][{}:{}:{} @{}]{}",
        GetTimeStr(std::chrono::system_clock::to_time_t(t)),
        (GetTimestampUs(t) % 1000000),
        kLvlNameArray[lvl],
        tid,
        file_name,
        line,
        column,
        function_name,
        std::string_view(log_data, log_data_size));

    fprintf(stderr, "%s\n", log_str.c_str());
  }
};

/**
 * @brief 简单异步日志记录器
 * 
 * 提供异步日志记录功能:
 * - 使用独立线程写入日志
 * - 通过阻塞队列传递日志消息
 * - 自动管理日志线程的生命周期
 * 
 * 示例:
 * @code
 *   SimpleAsyncLogger logger;
 *   logger.Log(kLogLevelInfo, __LINE__, 0, __FILE__,
 *              __FUNCTION__, "Async message", 13);
 * @endcode
 */
class SimpleAsyncLogger {
 public:
  SimpleAsyncLogger()
      : log_thread_(std::bind(&SimpleAsyncLogger::LogThread, this)) {}

  ~SimpleAsyncLogger() {
    queue_.Stop();
    if (log_thread_.joinable()) log_thread_.join();
  }

  SimpleAsyncLogger(const SimpleAsyncLogger&) = delete;
  SimpleAsyncLogger& operator=(const SimpleAsyncLogger&) = delete;

  static uint32_t GetLogLevel() { return 0; }

  void Log(uint32_t lvl,
           uint32_t line,
           uint32_t column,
           const char* file_name,
           const char* function_name,
           const char* log_data,
           size_t log_data_size) const {
    static constexpr std::string_view kLvlNameArray[] = {
        "Trace", "Debug", "Info", "Warn", "Error", "Fatal"};

    static constexpr uint32_t kLvlNameArraySize =
        sizeof(kLvlNameArray) / sizeof(kLvlNameArray[0]);
    [[unlikely]] if (lvl >= kLvlNameArraySize) 
      lvl = kLvlNameArraySize;

#if defined(_WIN32)
    thread_local size_t tid(std::hash<std::thread::id>{}(std::this_thread::get_id()));
#else
    thread_local size_t tid(gettid());
#endif

    auto t = std::chrono::system_clock::now();
    std::string log_str = ::aimrt_fmt::format(
        "[{}.{:0>6}][{}][{}][{}:{}:{} @{}]{}",
        GetTimeStr(std::chrono::system_clock::to_time_t(t)),
        (GetTimestampUs(t) % 1000000),
        kLvlNameArray[lvl],
        tid,
        file_name,
        line,
        column,
        function_name,
        std::string_view(log_data, log_data_size));
    try {
      queue_.Enqueue(std::move(log_str));
    } catch (...) {
    }
  }

 private:
  void LogThread() {
    while (true) {
      try {
        auto log_str = queue_.Dequeue();
        fprintf(stderr, "%s\n", log_str.c_str());
      } catch (...) {
        break;
      }
    }
  }

  mutable BlockQueue<std::string> queue_;
  std::thread log_thread_;
};

/**
 * @brief 日志记录器包装器
 * 
 * 提供统一的日志接口:
 * - 可以包装同步或异步日志记录器
 * - 支持自定义日志级别控制
 * - 支持自定义日志记录函数
 * 
 * 示例:
 * @code
 *   LoggerWrapper wrapper;
 *   wrapper.log_func = CustomLogger::Log;
 *   wrapper.Log(kLogLevelInfo, ...);
 * @endcode
 */
struct LoggerWrapper {
  uint32_t GetLogLevel() const {
    return get_log_level_func();
  }

  void Log(uint32_t lvl,
           uint32_t line,
           uint32_t column,
           const char* file_name,
           const char* function_name,
           const char* log_data,
           size_t log_data_size) const {
    log_func(lvl, line, column, file_name, function_name, log_data, log_data_size);
  }

  using GetLogLevelFunc = std::function<uint32_t(void)>;
  using LogFunc = std::function<void(uint32_t, uint32_t, uint32_t, const char*, const char*, const char*, size_t)>;

  GetLogLevelFunc get_log_level_func = SimpleLogger::GetLogLevel;
  LogFunc log_func = SimpleLogger::Log;
};

}  // namespace aimrt::common::util

/// Log with the specified logger handle
#define AIMRT_HANDLE_LOG(__lgr__, __lvl__, __fmt__, ...)                                 \
  do {                                                                                   \
    const auto& __cur_lgr__ = __lgr__;                                                   \
    if (__lvl__ >= __cur_lgr__.GetLogLevel()) {                                          \
      std::string __log_str__ = ::aimrt_fmt::format(__fmt__, ##__VA_ARGS__);             \
      constexpr auto __location__ = aimrt_src_loc::source_location::current();           \
      __cur_lgr__.Log(                                                                   \
          __lvl__, __location__.line(), __location__.column(), __location__.file_name(), \
          __FUNCTION__, __log_str__.c_str(), __log_str__.size());                        \
    }                                                                                    \
  } while (0)

/// Check and log with the specified logger handle
#define AIMRT_HANDLE_CHECK_LOG(__lgr__, __expr__, __lvl__, __fmt__, ...) \
  do {                                                                   \
    [[unlikely]] if (!(__expr__)) {                                     \
      AIMRT_HANDLE_LOG(__lgr__, __lvl__, __fmt__, ##__VA_ARGS__);      \
    }                                                                   \
  } while (0)

/// Log and throw with the specified logger handle
#define AIMRT_HANDLE_LOG_THROW(__lgr__, __lvl__, __fmt__, ...)                           \
  do {                                                                                   \
    std::string __log_str__ = ::aimrt_fmt::format(__fmt__, ##__VA_ARGS__);               \
    const auto& __cur_lgr__ = __lgr__;                                                   \
    if (__lvl__ >= __cur_lgr__.GetLogLevel()) {                                          \
      constexpr auto __location__ = aimrt_src_loc::source_location::current();                     \
      __cur_lgr__.Log(                                                                   \
          __lvl__, __location__.line(), __location__.column(), __location__.file_name(), \
          __FUNCTION__, __log_str__.c_str(), __log_str__.size());                        \
    }                                                                                    \
    throw aimrt::common::util::AimRTException(std::move(__log_str__));                   \
  } while (0)

/// Check and log and throw with the specified logger handle
#define AIMRT_HANDLE_CHECK_LOG_THROW(__lgr__, __expr__, __lvl__, __fmt__, ...) \
  do {                                                                         \
    [[unlikely]] if (!(__expr__))  {                                            \
      AIMRT_HANDLE_LOG_THROW(__lgr__, __lvl__, __fmt__, ##__VA_ARGS__);        \
    }                                                                          \
  } while (0)

#define AIMRT_HL_TRACE(__lgr__, __fmt__, ...) \
  AIMRT_HANDLE_LOG(__lgr__, aimrt::common::util::kLogLevelTrace, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_DEBUG(__lgr__, __fmt__, ...) \
  AIMRT_HANDLE_LOG(__lgr__, aimrt::common::util::kLogLevelDebug, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_INFO(__lgr__, __fmt__, ...) \
  AIMRT_HANDLE_LOG(__lgr__, aimrt::common::util::kLogLevelInfo, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_WARN(__lgr__, __fmt__, ...) \
  AIMRT_HANDLE_LOG(__lgr__, aimrt::common::util::kLogLevelWarn, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_ERROR(__lgr__, __fmt__, ...) \
  AIMRT_HANDLE_LOG(__lgr__, aimrt::common::util::kLogLevelError, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_FATAL(__lgr__, __fmt__, ...) \
  AIMRT_HANDLE_LOG(__lgr__, aimrt::common::util::kLogLevelFatal, __fmt__, ##__VA_ARGS__)

#define AIMRT_HL_CHECK_TRACE(__lgr__, __expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG(__lgr__, __expr__, aimrt::common::util::kLogLevelTrace, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_CHECK_DEBUG(__lgr__, __expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG(__lgr__, __expr__, aimrt::common::util::kLogLevelDebug, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_CHECK_INFO(__lgr__, __expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG(__lgr__, __expr__, aimrt::common::util::kLogLevelInfo, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_CHECK_WARN(__lgr__, __expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG(__lgr__, __expr__, aimrt::common::util::kLogLevelWarn, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_CHECK_ERROR(__lgr__, __expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG(__lgr__, __expr__, aimrt::common::util::kLogLevelError, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_CHECK_FATAL(__lgr__, __expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG(__lgr__, __expr__, aimrt::common::util::kLogLevelFatal, __fmt__, ##__VA_ARGS__)

#define AIMRT_HL_TRACE_THROW(__lgr__, __fmt__, ...) \
  AIMRT_HANDLE_LOG_THROW(__lgr__, aimrt::common::util::kLogLevelTrace, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_DEBUG_THROW(__lgr__, __fmt__, ...) \
  AIMRT_HANDLE_LOG_THROW(__lgr__, aimrt::common::util::kLogLevelDebug, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_INFO_THROW(__lgr__, __fmt__, ...) \
  AIMRT_HANDLE_LOG_THROW(__lgr__, aimrt::common::util::kLogLevelInfo, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_WARN_THROW(__lgr__, __fmt__, ...) \
  AIMRT_HANDLE_LOG_THROW(__lgr__, aimrt::common::util::kLogLevelWarn, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_ERROR_THROW(__lgr__, __fmt__, ...) \
  AIMRT_HANDLE_LOG_THROW(__lgr__, aimrt::common::util::kLogLevelError, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_FATAL_THROW(__lgr__, __fmt__, ...) \
  AIMRT_HANDLE_LOG_THROW(__lgr__, aimrt::common::util::kLogLevelFatal, __fmt__, ##__VA_ARGS__)

#define AIMRT_HL_CHECK_TRACE_THROW(__lgr__, __expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG_THROW(__lgr__, __expr__, aimrt::common::util::kLogLevelTrace, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_CHECK_DEBUG_THROW(__lgr__, __expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG_THROW(__lgr__, __expr__, aimrt::common::util::kLogLevelDebug, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_CHECK_INFO_THROW(__lgr__, __expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG_THROW(__lgr__, __expr__, aimrt::common::util::kLogLevelInfo, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_CHECK_WARN_THROW(__lgr__, __expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG_THROW(__lgr__, __expr__, aimrt::common::util::kLogLevelWarn, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_CHECK_ERROR_THROW(__lgr__, __expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG_THROW(__lgr__, __expr__, aimrt::common::util::kLogLevelError, __fmt__, ##__VA_ARGS__)
#define AIMRT_HL_CHECK_FATAL_THROW(__lgr__, __expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG_THROW(__lgr__, __expr__, aimrt::common::util::kLogLevelFatal, __fmt__, ##__VA_ARGS__)

/// Log with the default logger handle in current scope
#ifndef AIMRT_DEFAULT_LOGGER_HANDLE
  #define AIMRT_DEFAULT_LOGGER_HANDLE GetLogger()
#endif

#define AIMRT_TRACE(__fmt__, ...) \
  AIMRT_HANDLE_LOG(AIMRT_DEFAULT_LOGGER_HANDLE, aimrt::common::util::kLogLevelTrace, __fmt__, ##__VA_ARGS__)
#define AIMRT_DEBUG(__fmt__, ...) \
  AIMRT_HANDLE_LOG(AIMRT_DEFAULT_LOGGER_HANDLE, aimrt::common::util::kLogLevelDebug, __fmt__, ##__VA_ARGS__)
#define AIMRT_INFO(__fmt__, ...) \
  AIMRT_HANDLE_LOG(AIMRT_DEFAULT_LOGGER_HANDLE, aimrt::common::util::kLogLevelInfo, __fmt__, ##__VA_ARGS__)
#define AIMRT_WARN(__fmt__, ...) \
  AIMRT_HANDLE_LOG(AIMRT_DEFAULT_LOGGER_HANDLE, aimrt::common::util::kLogLevelWarn, __fmt__, ##__VA_ARGS__)
#define AIMRT_ERROR(__fmt__, ...) \
  AIMRT_HANDLE_LOG(AIMRT_DEFAULT_LOGGER_HANDLE, aimrt::common::util::kLogLevelError, __fmt__, ##__VA_ARGS__)
#define AIMRT_FATAL(__fmt__, ...) \
  AIMRT_HANDLE_LOG(AIMRT_DEFAULT_LOGGER_HANDLE, aimrt::common::util::kLogLevelFatal, __fmt__, ##__VA_ARGS__)

#define AIMRT_CHECK_TRACE(__expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG(AIMRT_DEFAULT_LOGGER_HANDLE, __expr__, aimrt::common::util::kLogLevelTrace, __fmt__, ##__VA_ARGS__)
#define AIMRT_CHECK_DEBUG(__expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG(AIMRT_DEFAULT_LOGGER_HANDLE, __expr__, aimrt::common::util::kLogLevelDebug, __fmt__, ##__VA_ARGS__)
#define AIMRT_CHECK_INFO(__expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG(AIMRT_DEFAULT_LOGGER_HANDLE, __expr__, aimrt::common::util::kLogLevelInfo, __fmt__, ##__VA_ARGS__)
#define AIMRT_CHECK_WARN(__expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG(AIMRT_DEFAULT_LOGGER_HANDLE, __expr__, aimrt::common::util::kLogLevelWarn, __fmt__, ##__VA_ARGS__)
#define AIMRT_CHECK_ERROR(__expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG(AIMRT_DEFAULT_LOGGER_HANDLE, __expr__, aimrt::common::util::kLogLevelError, __fmt__, ##__VA_ARGS__)
#define AIMRT_CHECK_FATAL(__expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG(AIMRT_DEFAULT_LOGGER_HANDLE, __expr__, aimrt::common::util::kLogLevelFatal, __fmt__, ##__VA_ARGS__)

#define AIMRT_TRACE_THROW(__fmt__, ...) \
  AIMRT_HANDLE_LOG_THROW(AIMRT_DEFAULT_LOGGER_HANDLE, aimrt::common::util::kLogLevelTrace, __fmt__, ##__VA_ARGS__)
#define AIMRT_DEBUG_THROW(__fmt__, ...) \
  AIMRT_HANDLE_LOG_THROW(AIMRT_DEFAULT_LOGGER_HANDLE, aimrt::common::util::kLogLevelDebug, __fmt__, ##__VA_ARGS__)
#define AIMRT_INFO_THROW(__fmt__, ...) \
  AIMRT_HANDLE_LOG_THROW(AIMRT_DEFAULT_LOGGER_HANDLE, aimrt::common::util::kLogLevelInfo, __fmt__, ##__VA_ARGS__)
#define AIMRT_WARN_THROW(__fmt__, ...) \
  AIMRT_HANDLE_LOG_THROW(AIMRT_DEFAULT_LOGGER_HANDLE, aimrt::common::util::kLogLevelWarn, __fmt__, ##__VA_ARGS__)
#define AIMRT_ERROR_THROW(__fmt__, ...) \
  AIMRT_HANDLE_LOG_THROW(AIMRT_DEFAULT_LOGGER_HANDLE, aimrt::common::util::kLogLevelError, __fmt__, ##__VA_ARGS__)
#define AIMRT_FATAL_THROW(__fmt__, ...) \
  AIMRT_HANDLE_LOG_THROW(AIMRT_DEFAULT_LOGGER_HANDLE, aimrt::common::util::kLogLevelFatal, __fmt__, ##__VA_ARGS__)

#define AIMRT_CHECK_TRACE_THROW(__expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG_THROW(AIMRT_DEFAULT_LOGGER_HANDLE, __expr__, aimrt::common::util::kLogLevelTrace, __fmt__, ##__VA_ARGS__)
#define AIMRT_CHECK_DEBUG_THROW(__expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG_THROW(AIMRT_DEFAULT_LOGGER_HANDLE, __expr__, aimrt::common::util::kLogLevelDebug, __fmt__, ##__VA_ARGS__)
#define AIMRT_CHECK_INFO_THROW(__expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG_THROW(AIMRT_DEFAULT_LOGGER_HANDLE, __expr__, aimrt::common::util::kLogLevelInfo, __fmt__, ##__VA_ARGS__)
#define AIMRT_CHECK_WARN_THROW(__expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG_THROW(AIMRT_DEFAULT_LOGGER_HANDLE, __expr__, aimrt::common::util::kLogLevelWarn, __fmt__, ##__VA_ARGS__)
#define AIMRT_CHECK_ERROR_THROW(__expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG_THROW(AIMRT_DEFAULT_LOGGER_HANDLE, __expr__, aimrt::common::util::kLogLevelError, __fmt__, ##__VA_ARGS__)
#define AIMRT_CHECK_FATAL_THROW(__expr__, __fmt__, ...) \
  AIMRT_HANDLE_CHECK_LOG_THROW(AIMRT_DEFAULT_LOGGER_HANDLE, __expr__, aimrt::common::util::kLogLevelFatal, __fmt__, ##__VA_ARGS__)
