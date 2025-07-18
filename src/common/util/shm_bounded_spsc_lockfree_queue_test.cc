#include "util/shm_bounded_spsc_lockfree_queue.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <string>

namespace omnirt::common::util {
namespace {

/**
 * @brief 基于共享内存的有界单生产者单消费者无锁队列的测试类
 * 
 * 该测试类验证ShmBoundedSpscLockfreeQueue的以下特性：
 * - 初始化参数的有效性检查
 * - 基本的入队出队操作
 * - 队列满/空状态处理
 * - 共享内存的创建和附加
 */
class ShmBoundedSpscLockfreeQueueTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 确保测试前共享内存被清理
    shm_unlink("/test_queue");
  }

  void TearDown() override {
    // 确保测试后共享内存被清理
    shm_unlink("/test_queue");
  }

  ShmBoundedSpscLockfreeQueue<int> queue_;
};

/**
 * @brief 测试共享内存队列的初始化功能
 * 
 * 测试场景：
 * 1. 有效初始化：验证正常容量的初始化
 * 2. 无效初始化：测试容量为0、无效共享内存名等情况
 * 3. 2的幂次要求：测试是否符合容量必须为2的幂次的约束
 * 
 * 验证点：
 * - 初始化后的容量是否正确
 * - 初始状态是否为空
 * - 对无效参数的处理是否合理
 */
TEST_F(ShmBoundedSpscLockfreeQueueTest, InitializationTest) {
  // 测试有效初始化
  EXPECT_TRUE(queue_.Init("/test_queue", 16));
  EXPECT_EQ(queue_.GetShmState(), ShmBoundedSpscLockfreeQueue<int>::ShmState::CREATED);
  EXPECT_EQ(queue_.Capacity(), 16);
  EXPECT_TRUE(queue_.Empty());
  EXPECT_EQ(queue_.Size(), 0);

  // 测试无效大小(0)
  ShmBoundedSpscLockfreeQueue<int> queue2;
  EXPECT_FALSE(queue2.Init("/test_queue2", 0));

  // 测试无效共享内存名称
  ShmBoundedSpscLockfreeQueue<int> queue3;
  EXPECT_FALSE(queue3.Init("invalid_name", 16));  // 必须以/开头

  // 测试2的幂次要求
  ShmBoundedSpscLockfreeQueue<int> queue4;
  EXPECT_TRUE(queue4.Init("/test_queue4", 10, false));  // 不强制2的幂次
  queue4.Close();
  
  ShmBoundedSpscLockfreeQueue<int> queue5;
  EXPECT_FALSE(queue5.Init("/test_queue5", 10, true));  // 强制2的幂次会失败
  
  ShmBoundedSpscLockfreeQueue<int> queue6;
  EXPECT_TRUE(queue6.Init("/test_queue6", 16, true));  // 16是2的幂次，应成功
}

/**
 * @brief 测试附加到已存在共享内存
 * 
 * 测试场景：
 * - 创建共享内存并写入数据
 * - 第二个队列附加到同一共享内存
 * - 验证数据可在两个队列实例间共享
 */
TEST_F(ShmBoundedSpscLockfreeQueueTest, AttachToExistingTest) {
  // 创建者
  ShmBoundedSpscLockfreeQueue<int> creator;
  ASSERT_TRUE(creator.Init("/test_attach", 16, false, true));
  EXPECT_EQ(creator.GetShmState(), ShmBoundedSpscLockfreeQueue<int>::ShmState::CREATED);
  
  // 写入数据
  EXPECT_TRUE(creator.Enqueue(42));
  EXPECT_TRUE(creator.Enqueue(43));
  EXPECT_EQ(creator.Size(), 2);
  
  // 附加者
  ShmBoundedSpscLockfreeQueue<int> attacher;
  ASSERT_TRUE(attacher.Init("/test_attach", 16, false, false));
  EXPECT_EQ(attacher.GetShmState(), ShmBoundedSpscLockfreeQueue<int>::ShmState::ATTACHED);
  
  // 验证附加者可以看到相同的数据
  EXPECT_EQ(attacher.Size(), 2);
  
  int value;
  EXPECT_TRUE(attacher.Dequeue(&value));
  EXPECT_EQ(value, 42);
  EXPECT_TRUE(attacher.Dequeue(&value));
  EXPECT_EQ(value, 43);
  EXPECT_TRUE(attacher.Empty());
  
  // 清理
  creator.Close();
  attacher.Close();
  shm_unlink("/test_attach");
}

/**
 * @brief 测试队列的基本操作
 * 
 * 测试要点：
 * - 入队操作的正确性
 * - 出队操作的正确性
 * - 队列状态（大小、是否为空）的准确性
 * - 空队列出队的处理
 */
TEST_F(ShmBoundedSpscLockfreeQueueTest, BasicOperations) {
  ASSERT_TRUE(queue_.Init("/test_queue", 4));
  
  // 测试入队
  EXPECT_TRUE(queue_.Enqueue(1));
  EXPECT_EQ(queue_.Size(), 1);
  EXPECT_FALSE(queue_.Empty());

  // 测试出队
  int value;
  EXPECT_TRUE(queue_.Dequeue(&value));
  EXPECT_EQ(value, 1);
  EXPECT_TRUE(queue_.Empty());
  EXPECT_EQ(queue_.Size(), 0);

  // 测试从空队列出队
  EXPECT_FALSE(queue_.Dequeue(&value));
}

/**
 * @brief 测试队列满时的行为
 * 
 * 测试要点：
 * - 验证队列满时的入队操作
 * - 测试EnqueueOverwrite的覆盖写入功能
 * - 确保数据完整性和顺序正确性
 */
TEST_F(ShmBoundedSpscLockfreeQueueTest, QueueFullBehavior) {
  ASSERT_TRUE(queue_.Init("/test_queue", 2));

  EXPECT_TRUE(queue_.Enqueue(1));
  EXPECT_TRUE(queue_.Enqueue(2));
  EXPECT_FALSE(queue_.Enqueue(3));  // 队列已满

  // 测试EnqueueOverwrite
  EXPECT_TRUE(queue_.EnqueueOverwrite(3));
  
  int value;
  EXPECT_TRUE(queue_.Dequeue(&value));
  EXPECT_EQ(value, 2);  // 第一个值应该是2
  EXPECT_TRUE(queue_.Dequeue(&value));
  EXPECT_EQ(value, 3);  // 第二个值应该是覆盖后的3
}

/**
 * @brief 测试获取最新数据功能
 * 
 * 测试要点：
 * - 验证DequeueLatest的功能正确性
 * - 确保能够跳过旧数据直接获取最新值
 * - 验证操作后队列状态的正确性
 */
TEST_F(ShmBoundedSpscLockfreeQueueTest, DequeueLatest) {
  ASSERT_TRUE(queue_.Init("/test_queue", 4));

  // 填充队列
  EXPECT_TRUE(queue_.Enqueue(1));
  EXPECT_TRUE(queue_.Enqueue(2));
  EXPECT_TRUE(queue_.Enqueue(3));

  int value;
  EXPECT_TRUE(queue_.DequeueLatest(&value));
  EXPECT_EQ(value, 3);  // 应该获取最新值
  EXPECT_TRUE(queue_.Empty());  // DequeueLatest后队列应为空
}

/**
 * @brief 结构体类型测试
 * 
 * 测试在共享内存队列中使用复杂数据类型的能力
 */
TEST_F(ShmBoundedSpscLockfreeQueueTest, StructTypeTest) {
  struct TestData {
    int id;
    double value;
    char name[32];
    
    // 重写比较运算符以便于测试验证
    bool operator==(const TestData& other) const {
      return id == other.id && 
             std::abs(value - other.value) < 0.0001 && 
             strcmp(name, other.name) == 0;
    }
  };
  
  ShmBoundedSpscLockfreeQueue<TestData> struct_queue;
  ASSERT_TRUE(struct_queue.Init("/test_struct_queue", 4));
  
  TestData data1{1, 3.14, "test1"};
  TestData data2{2, 2.71, "test2"};
  
  EXPECT_TRUE(struct_queue.Enqueue(data1));
  EXPECT_TRUE(struct_queue.Enqueue(data2));
  
  TestData result;
  EXPECT_TRUE(struct_queue.Dequeue(&result));
  EXPECT_EQ(result, data1);
  
  EXPECT_TRUE(struct_queue.Dequeue(&result));
  EXPECT_EQ(result, data2);
  
  struct_queue.Close();
  shm_unlink("/test_struct_queue");
}

}  // namespace
}  // namespace omnirt::common::util
