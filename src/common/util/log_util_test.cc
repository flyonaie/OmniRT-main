// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include "util/log_util.h"

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace aimrt::common::util;

inline aimrt::common::util::SimpleLogger& GetLogger() {
  static aimrt::common::util::SimpleLogger logger;
  return logger;
}

class LogUtilTest : public ::testing::Test {
 protected:
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
  
  // std::string output = GetLogOutput();
  std::cout << "2311111" << std::endl;

  AIMRT_INFO("ns_timestamp: %lu, ns_tp: %lu", 12, 222);


  // std::cout << output;  // 打印output

  // EXPECT_TRUE(output.find("[Info]") != std::string::npos);
  // EXPECT_TRUE(output.find("test.cpp") != std::string::npos);
  // EXPECT_TRUE(output.find("TestFunc") != std::string::npos);
  // EXPECT_TRUE(output.find("测试消息") != std::string::npos);
}
