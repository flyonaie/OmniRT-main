// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.
//
// LightSignal类的单元测试
// 验证LightSignal类的基本功能和线程同步特性
//
// 测试内容:
// 1. 基本的信号发送和等待功能
// 2. 多线程环境下的同步行为
// 3. Reset功能的正确性
// 4. 线程间的数据同步
//
// 测试策略:
// - 使用两个线程模拟信号发送和接收
// - 通过共享变量验证同步正确性
// - 使用断言确保执行顺序符合预期

#include <gtest/gtest.h>
#include <thread>
#include "util/light_signal.h"

namespace aimrt::common::util {

/**
 * @brief 测试LightSignal的基本功能
 * 
 * 测试场景:
 * 1. 创建两个线程t1和t2
 * 2. t1等待信号，t2发送信号
 * 3. 通过共享变量i验证同步顺序
 * 
 * 测试步骤:
 * 1. t1首先等待，此时i=0
 * 2. t2休眠100ms后设置i=1并发送信号
 * 3. t1收到信号后验证i=1
 * 4. t1重置信号并再次等待
 * 5. t2休眠100ms后设置i=2并发送信号
 * 6. t1收到信号后验证i=2
 * 
 * 预期结果:
 * - 所有断言都应该通过
 * - 验证了Wait、Notify和Reset的正确性
 * - 确保了线程间的正确同步
 */
TEST(LightSignalTest, base) {
  LightSignal s;
  uint32_t i = 0;

  // 等待线程：验证信号和共享变量
  std::thread t1([&] {
    ASSERT_EQ(i, 0);  // 初始状态检查
    s.Wait();         // 等待第一个信号
    ASSERT_EQ(i, 1);  // 验证第一次同步
    s.Reset();        // 重置信号
    s.Wait();         // 等待第二个信号
    ASSERT_EQ(i, 2);  // 验证第二次同步
  });

  // 通知线程：设置共享变量并发送信号
  std::thread t2([&] {
    ASSERT_EQ(i, 0);  // 初始状态检查
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 确保t1先等待
    i = 1;            // 设置第一个值
    s.Notify();       // 发送第一个信号
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 等待t1处理
    i = 2;            // 设置第二个值
    s.Notify();       // 发送第二个信号
  });

  t1.join();  // 等待测试线程完成
  t2.join();
}

}  // namespace aimrt::common::util