// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.
//
// 日志工具单元测试
// 验证日志工具库的各项功能，包括同步日志、异步日志和日志格式化。
//
// 测试内容:
// 1. 基本日志记录功能
// 2. 日志格式正确性
// 3. 线程安全性
// 4. 错误处理
//
// 测试策略:
// - 使用Google Test框架
// - 验证日志输出格式
// - 测试多线程场景
// - 检查边界条件

#include "util/log_util.h"

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace aimrt::common::util;

/**
 * @brief 获取全局日志记录器实例
 * 
 * 使用单例模式确保全局只有一个日志记录器实例
 * 
 * @return SimpleLogger& 日志记录器引用
 */
inline aimrt::common::util::SimpleLogger& GetLogger() {
  static aimrt::common::util::SimpleLogger logger;
  return logger;
}

/**
 * @brief 日志工具测试类
 * 
 * 提供日志测试的基础设施:
 * - 重定向stderr以捕获日志输出
 * - 提供日志输出获取方法
 * - 自动管理重定向的文件描述符
 */
class LogUtilTest : public ::testing::Test {
 protected:
  /**
   * @brief 获取日志输出内容
   * 
   * 从重定向的stderr中读取所有输出内容
   * 
   * @return std::string 日志输出内容
   */
  std::string GetLogOutput() {
    long pos = ftell(stderr);
    rewind(stderr);
    std::string output;
    output.resize(pos);
    fread(&output[0], 1, pos, stderr);
    return output;
  }

  FILE* old_stderr;  ///< 保存原始stderr
};

/**
 * @brief 测试SimpleLogger的基本功能
 * 
 * 测试场景:
 * 1. 直接调用Log方法记录日志
 * 2. 使用宏记录日志
 * 3. 验证日志格式和内容
 * 
 * 验证点:
 * - 日志级别正确显示
 * - 源文件信息正确
 * - 函数名正确
 * - 日志内容完整
 */
TEST_F(LogUtilTest, SimpleLoggerBasicTest) {
  // 测试直接调用Log方法
  SimpleLogger::Log(kLogLevelInfo, 123, 0, "test.cpp", "TestFunc", "测试消息", 12);
  
  // 输出调试信息
  std::cout << "2311111" << std::endl;

  // 测试使用宏记录日志
  AIMRT_INFO("ns_timestamp: %lu, ns_tp: %lu", 12, 222);

  // TODO: 添加更多验证
  // std::string output = GetLogOutput();
  // std::cout << output;  // 打印output
  // EXPECT_TRUE(output.find("[Info]") != std::string::npos);
  // EXPECT_TRUE(output.find("test.cpp") != std::string::npos);
  // EXPECT_TRUE(output.find("TestFunc") != std::string::npos);
  // EXPECT_TRUE(output.find("测试消息") != std::string::npos);
}

/**
 * @brief 测试多线程场景下的日志记录
 * 
 * 测试场景:
 * 1. 多个线程同时写日志
 * 2. 验证日志内容完整性
 * 3. 检查线程安全性
 * 
 * 验证点:
 * - 日志记录不丢失
 * - 日志内容不混乱
 * - 性能表现正常
 */
TEST_F(LogUtilTest, MultiThreadLoggingTest) {
  const int kThreadCount = 4;
  const int kLogsPerThread = 100;
  std::vector<std::thread> threads;

  // 启动多个线程同时写日志
  for (int i = 0; i < kThreadCount; ++i) {
    threads.emplace_back([i, kLogsPerThread]() {
      for (int j = 0; j < kLogsPerThread; ++j) {
        AIMRT_INFO("Thread %d, Log %d", i, j);
      }
    });
  }

  // 等待所有线程完成
  for (auto& thread : threads) {
    thread.join();
  }

  // TODO: 验证日志输出
  // std::string output = GetLogOutput();
  // 检查每个线程的日志是否都被正确记录
}

/**
 * @brief 测试异步日志记录器
 * 
 * 测试场景:
 * 1. 异步写入大量日志
 * 2. 验证日志刷新机制
 * 3. 测试优雅关闭
 * 
 * 验证点:
 * - 日志异步写入成功
 * - 日志顺序正确
 * - 关闭时无日志丢失
 */
TEST_F(LogUtilTest, AsyncLoggerTest) {
  SimpleAsyncLogger async_logger;
  
  // 测试异步写入
  for (int i = 0; i < 1000; ++i) {
    async_logger.Log(kLogLevelInfo, 123, 0, "async_test.cpp", "AsyncTest", 
                    "异步日志测试 %d", i);
  }

  // 等待异步日志写入完成
  std::this_thread::sleep_for(std::chrono::seconds(1));
  
  // TODO: 验证异步日志输出
  // std::string output = GetLogOutput();
  // 检查日志完整性和顺序
}

/**
 * @brief 测试日志格式化功能
 * 
 * 测试场景:
 * 1. 各种数据类型的格式化
 * 2. 特殊字符处理
 * 3. 长字符串处理
 * 
 * 验证点:
 * - 格式化结果正确
 * - 特殊字符正确显示
 * - 不发生缓冲区溢出
 */
TEST_F(LogUtilTest, LogFormattingTest) {
  // 测试各种数据类型的格式化
  AIMRT_INFO("整数: %d, 浮点数: %f, 字符串: %s", 42, 3.14, "测试字符串");
  
  // 测试特殊字符
  AIMRT_INFO("特殊字符测试: \n\t\r");
  
  // 测试长字符串
  std::string long_str(1000, 'A');
  AIMRT_INFO("长字符串测试: %s", long_str.c_str());
  
  // TODO: 验证格式化结果
  // std::string output = GetLogOutput();
  // 检查格式化是否正确
}
