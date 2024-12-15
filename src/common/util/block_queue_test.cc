// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include <gtest/gtest.h>
#include <thread>
#include "util/block_queue.h"

namespace aimrt::common::util {

/**
 * @brief 测试BlockQueue的基本入队和出队功能
 * 
 * 测试要点：
 * - 验证单个元素的入队操作
 * - 验证单个元素的出队操作
 * - 确保出队元素与入队元素一致
 */
TEST(BlockQueueTest, EnqueueDequeue) {
  BlockQueue<int> queue;
  queue.Enqueue(1);
  ASSERT_EQ(1, queue.Dequeue());
}

/**
 * @brief 测试空队列的TryDequeue操作
 * 
 * 测试要点：
 * - 验证空队列调用TryDequeue时返回nullopt
 * - 确保非阻塞操作的正确性
 */
TEST(BlockQueueTest, TryDequeueEmpty) {
  BlockQueue<int> queue;
  auto result = queue.TryDequeue();
  ASSERT_EQ(std::nullopt, result);
}

/**
 * @brief 测试非空队列的TryDequeue操作
 * 
 * 测试要点：
 * - 验证队列非空时TryDequeue能正确返回元素
 * - 验证返回值包含正确的数据
 * - 确保optional包装的正确性
 */
TEST(BlockQueueTest, TryDequeueNonEmpty) {
  BlockQueue<int> queue;
  queue.Enqueue(1);
  auto result = queue.TryDequeue();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(1, result.value());
}

/**
 * @brief 测试BlockQueue的多线程并发操作
 * 
 * 测试要点：
 * - 验证在多线程环境下的数据一致性
 * - 测试生产者线程的入队操作
 * - 测试消费者线程的出队操作
 * - 确保线程安全性和同步机制的正确性
 * 
 * 测试场景：
 * 1. 一个生产者线程连续入队5个元素
 * 2. 一个消费者线程连续出队5个元素
 * 3. 验证所有操作都能正确完成
 */
TEST(BlockQueueTest, MultipleThreads) {
  BlockQueue<int> queue;
  std::vector<std::thread> threads;

  threads.emplace_back([&]() {
    for (int i = 0; i < 5; ++i) {
      queue.Enqueue(i);
    }
  });

  threads.emplace_back([&]() {
    for (int i = 0; i < 5; ++i) {
      auto item = queue.Dequeue();
      ASSERT_GE(item, 0);
    }
  });

  for (auto& t : threads) {
    t.join();
  }
}

/**
 * @brief 测试BlockQueue的停止功能
 * 
 * 测试要点：
 * - 验证队列停止后的异常处理机制
 * - 测试入队操作抛出预期异常
 * - 测试出队操作抛出预期异常
 * 
 * 异常验证：
 * - 入队操作应抛出BlockQueueStoppedException
 * - 出队操作应抛出BlockQueueStoppedException
 */
TEST(BlockQueueTest, Stop) {
  BlockQueue<int> queue;

  queue.Stop();

  ASSERT_THROW(queue.Enqueue(1), BlockQueueStoppedException);
  ASSERT_THROW(queue.Dequeue(), BlockQueueStoppedException);
}

}  // namespace aimrt::common::util
