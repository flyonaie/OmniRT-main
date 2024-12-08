// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include <gtest/gtest.h>

#include "util/deferred.h"

namespace aimrt::common::util {

TEST(DEFERRED_TEST, Basic_test) {
  bool executed = false;
  {
    Deferred defer([&executed]() { executed = true; });
    EXPECT_FALSE(executed);  // 在作用域内还未执行
  }
  EXPECT_TRUE(executed);  // 离开作用域后应该被执行
}

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

TEST(DEFERRED_TEST, DefaultConstruction_test) {
  Deferred defer;  // 默认构造
  EXPECT_FALSE(static_cast<bool>(defer));  // 应该是空的
  defer.Dismiss();  // 对空对象调用Dismiss应该是安全的
  EXPECT_FALSE(static_cast<bool>(defer));
}

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
