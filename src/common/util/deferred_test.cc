// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.
//
// Deferred类的单元测试
// 测试内容包括:
// 1. 基本功能测试
// 2. 移动语义测试
// 3. 取消执行测试
// 4. 多重延迟操作测试
// 5. 异常安全性测试
//
// 这些测试用例覆盖了Deferred类的所有主要功能和边界情况,
// 确保其在各种使用场景下都能正确工作。

#include <gtest/gtest.h>

#include "util/deferred.h"

namespace aimrt::common::util {

/**
 * @brief 测试Deferred的基本功能
 * 
 * 验证:
 * 1. 在作用域结束时是否正确执行预定操作
 * 2. 在作用域内是否不会提前执行
 */
TEST(DEFERRED_TEST, Basic_test) {
  bool executed = false;
  {
    Deferred defer([&executed]() { executed = true; });
    EXPECT_FALSE(executed);  // 在作用域内还未执行
  }
  EXPECT_TRUE(executed);  // 离开作用域后应该被执行
}

/**
 * @brief 测试Deferred的移动语义
 * 
 * 验证:
 * 1. 移动后源对象状态
 * 2. 移动后目标对象状态
 * 3. 移动后的执行时机
 */
TEST(DEFERRED_TEST, Move_test) {
  bool executed = false;
  {
    Deferred defer1([&executed]() { executed = true; });
    {
      Deferred defer2(std::move(defer1));
      EXPECT_FALSE(executed);
      EXPECT_FALSE(static_cast<bool>(defer1));  // 移动后原对象应该为空
      EXPECT_TRUE(static_cast<bool>(defer2));   // 移动后新对象应该有效
    }
    EXPECT_TRUE(executed);  // defer2离开作用域时应该执行
  }
}

/**
 * @brief 测试Deferred的取消功能
 * 
 * 验证:
 * 1. Dismiss()后是否正确取消执行
 * 2. 对象状态是否正确更新
 */
TEST(DEFERRED_TEST, Dismiss_test) {
  bool executed = false;
  {
    Deferred defer([&executed]() { executed = true; });
    EXPECT_TRUE(static_cast<bool>(defer));
    defer.Dismiss();  // 取消执行
    EXPECT_FALSE(static_cast<bool>(defer));
  }
  EXPECT_FALSE(executed);  // 被取消后不应该执行
}

/**
 * @brief 测试多个Deferred对象的执行顺序
 * 
 * 验证:
 * 1. 多个Deferred对象的执行顺序是否符合LIFO
 * 2. 计数器值是否正确累加
 */
TEST(DEFERRED_TEST, MultipleActions_test) {
  int counter = 0;
  {
    Deferred defer1([&counter]() { counter += 1; });
    {
      Deferred defer2([&counter]() { counter += 2; });
      EXPECT_EQ(counter, 0);
    }
    EXPECT_EQ(counter, 2);  // defer2应该先执行
  }
  EXPECT_EQ(counter, 3);    // defer1应该后执行
}

/**
 * @brief 测试Deferred的移动赋值
 * 
 * 验证:
 * 1. 移动赋值后源对象的动作是否被替换
 * 2. 原有动作是否被正确处理
 * 3. 新动作是否在正确时机执行
 */
TEST(DEFERRED_TEST, MoveAssignment_test) {
  bool executed1 = false;
  bool executed2 = false;
  {
    Deferred defer1([&executed1]() { executed1 = true; });
    {
      Deferred defer2([&executed2]() { executed2 = true; });
      defer1 = std::move(defer2);  // defer2的动作应该替换defer1的动作
      EXPECT_FALSE(executed1);
      EXPECT_FALSE(executed2);
    }
    EXPECT_FALSE(executed1);  // 原来的动作不应该执行
    EXPECT_FALSE(executed2);  // 还未到作用域结束
  }
  EXPECT_FALSE(executed1);
  EXPECT_TRUE(executed2);     // 只有新的动作应该执行
}

/**
 * @brief 测试Deferred的默认构造
 * 
 * 验证:
 * 1. 默认构造的对象状态
 * 2. 对空对象调用Dismiss的安全性
 */
TEST(DEFERRED_TEST, DefaultConstruction_test) {
  Deferred defer;  // 默认构造
  EXPECT_FALSE(static_cast<bool>(defer));  // 应该是空的
  defer.Dismiss();  // 对空对象调用Dismiss应该是安全的
  EXPECT_FALSE(static_cast<bool>(defer));
}

/**
 * @brief 测试Deferred的异常安全性
 * 
 * 验证:
 * 1. 发生异常时是否仍能正确执行清理操作
 * 2. RAII机制是否在异常情况下正常工作
 */
TEST(DEFERRED_TEST, ExceptionSafety_test) {
  bool executed = false;
  try {
    Deferred defer([&executed]() { executed = true; });
    throw std::runtime_error("test exception");
  } catch (const std::exception&) {
    // 即使发生异常，析构函数也应该被调用
  }
  EXPECT_TRUE(executed);
}

}  // namespace aimrt::common::util
