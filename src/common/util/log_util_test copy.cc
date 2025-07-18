// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include "util/log_util.h"

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace aimrt::common::util;

class LogUtilTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 重定向stderr到临时文件以便测试输出
    old_stderr = stderr;
    stderr = tmpfile();
  }

  void TearDown() override {
    if (stderr != old_stderr) {
      fclose(stderr);
      stderr = old_stderr;
    }
  }

  std::string GetLogOutput() {
    long pos = ftell(stderr);
    rewind(stderr);
    std::string output;
    output.resize(pos);
    fread(&output[0], 1, pos, stderr);
    return output;
  }

  FILE* old_stderr;
};

TEST_F(LogUtilTest, SimpleLoggerBasicTest) {
  SimpleLogger::Log(kLogLevelInfo, 123, 0, "test.cpp", "TestFunc", "测试消息", 12);
  
  std::string output = GetLogOutput();
  std::cout << "2311111" << std::endl;
  std::cout << output;  // 打印output

  EXPECT_TRUE(output.find("[Info]") != std::string::npos);
  EXPECT_TRUE(output.find("test.cpp") != std::string::npos);
  EXPECT_TRUE(output.find("TestFunc") != std::string::npos);
  EXPECT_TRUE(output.find("测试消息") != std::string::npos);
}

TEST_F(LogUtilTest, SimpleAsyncLoggerBasicTest) {
  SimpleAsyncLogger logger;
  
  logger.Log(kLogLevelWarn, 456, 0, "async_test.cpp", "AsyncTestFunc", "异步测试消息", 15);
  
  // 等待异步日志写入
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  
  std::string output = GetLogOutput();
  EXPECT_TRUE(output.find("[Warn]") != std::string::npos);
  EXPECT_TRUE(output.find("async_test.cpp") != std::string::npos);
  EXPECT_TRUE(output.find("AsyncTestFunc") != std::string::npos);
  EXPECT_TRUE(output.find("异步测试消息") != std::string::npos);
}

TEST_F(LogUtilTest, LoggerWrapperTest) {
  LoggerWrapper wrapper;
  
  wrapper.Log(kLogLevelError, 789, 0, "wrapper_test.cpp", "WrapperTestFunc", "包装器测试", 12);
  
  std::string output = GetLogOutput();
  EXPECT_TRUE(output.find("[Error]") != std::string::npos);
  EXPECT_TRUE(output.find("wrapper_test.cpp") != std::string::npos);
  EXPECT_TRUE(output.find("WrapperTestFunc") != std::string::npos);
  EXPECT_TRUE(output.find("包装器测试") != std::string::npos);
}

TEST_F(LogUtilTest, MacroTest) {
  LoggerWrapper logger;
  
  AIMRT_HL_INFO(logger, "宏测试消息 {}", 42);
  
  std::string output = GetLogOutput();
  EXPECT_TRUE(output.find("[Info]") != std::string::npos);
  EXPECT_TRUE(output.find("宏测试消息 42") != std::string::npos);
}

TEST_F(LogUtilTest, CheckMacroTest) {
  LoggerWrapper logger;
  bool condition = false;
  
  AIMRT_HL_CHECK_WARN(logger, condition, "条件检查失败 {}", "测试");
  
  std::string output = GetLogOutput();
  EXPECT_TRUE(output.find("[Warn]") != std::string::npos);
  EXPECT_TRUE(output.find("条件检查失败 测试") != std::string::npos);
}

TEST_F(LogUtilTest, ThrowMacroTest) {
  LoggerWrapper logger;
  
  EXPECT_THROW({
    AIMRT_HL_ERROR_THROW(logger, "抛出错误 {}", "测试异常");
  }, aimrt::common::util::AimRTException);
  
  std::string output = GetLogOutput();
  EXPECT_TRUE(output.find("[Error]") != std::string::npos);
  EXPECT_TRUE(output.find("抛出错误 测试异常") != std::string::npos);
} 