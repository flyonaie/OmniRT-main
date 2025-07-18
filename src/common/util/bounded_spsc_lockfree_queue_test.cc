#include "util/bounded_spsc_lockfree_queue.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>

namespace omnirt::common::util {
namespace {

/**
 * @brief 有界单生产者单消费者无锁队列的测试类
 * 
 * 该测试类验证BoundedSpscLockfreeQueue的以下特性：
 * - 初始化参数的有效性检查
 * - 基本的入队出队操作
 * - 队列满/空状态处理
 * - 并发安全性
 * - 性能表现
 */
class BoundedSpscLockfreeQueueTest : public ::testing::Test {
 protected:
  BoundedSpscLockfreeQueue<int> queue_;
};

/**
 * @brief 测试队列初始化功能
 * 
 * 测试场景：
 * 1. 有效初始化：验证正常容量的初始化
 * 2. 无效初始化：测试容量为0的情况
 * 3. 2的幂次要求：测试是否符合容量必须为2的幂次的约束
 * 
 * 验证点：
 * - 初始化后的容量是否正确
 * - 初始状态是否为空
 * - 对无效参数的处理是否合理
 */
TEST_F(BoundedSpscLockfreeQueueTest, InitializationTest) {
  // Test valid initialization
  EXPECT_TRUE(queue_.Init(16));
  EXPECT_EQ(queue_.Capacity(), 16);
  EXPECT_TRUE(queue_.Empty());
  EXPECT_EQ(queue_.Size(), 0);

  // Test invalid size (0)
  BoundedSpscLockfreeQueue<int> queue2;
  EXPECT_FALSE(queue2.Init(0));

  // Test power of two requirement
  BoundedSpscLockfreeQueue<int> queue3;
  EXPECT_TRUE(queue3.Init(10, false));  // Should work without power of two requirement
  
  BoundedSpscLockfreeQueue<int> queue4;
  EXPECT_FALSE(queue4.Init(10, true));  // Should fail with power of two requirement
  EXPECT_TRUE(queue4.Init(16, true));   // Should work with power of two
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
TEST_F(BoundedSpscLockfreeQueueTest, BasicOperations) {
  ASSERT_TRUE(queue_.Init(4));
  
  // Test enqueue
  EXPECT_TRUE(queue_.Enqueue(1));
  EXPECT_EQ(queue_.Size(), 1);
  EXPECT_FALSE(queue_.Empty());

  // Test dequeue
  int value;
  EXPECT_TRUE(queue_.Dequeue(&value));
  EXPECT_EQ(value, 1);
  EXPECT_TRUE(queue_.Empty());
  EXPECT_EQ(queue_.Size(), 0);

  // Test dequeue from empty queue
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
TEST_F(BoundedSpscLockfreeQueueTest, QueueFullBehavior) {
  ASSERT_TRUE(queue_.Init(2));

  EXPECT_TRUE(queue_.Enqueue(1));
  EXPECT_TRUE(queue_.Enqueue(2));
  EXPECT_FALSE(queue_.Enqueue(3));  // Queue is full

  // Test EnqueueOverwrite
  EXPECT_TRUE(queue_.EnqueueOverwrite(3));
  
  int value;
  EXPECT_TRUE(queue_.Dequeue(&value));
  EXPECT_EQ(value, 2);  // First value should be 2
  EXPECT_TRUE(queue_.Dequeue(&value));
  EXPECT_EQ(value, 3);  // Second value should be overwritten to 3
}

/**
 * @brief 测试获取最新数据功能
 * 
 * 测试要点：
 * - 验证DequeueLatest的功能正确性
 * - 确保能够跳过旧数据直接获取最新值
 * - 验证操作后队列状态的正确性
 */
TEST_F(BoundedSpscLockfreeQueueTest, DequeueLatest) {
  ASSERT_TRUE(queue_.Init(4));

  // Fill queue
  EXPECT_TRUE(queue_.Enqueue(1));
  EXPECT_TRUE(queue_.Enqueue(2));
  EXPECT_TRUE(queue_.Enqueue(3));

  int value;
  EXPECT_TRUE(queue_.DequeueLatest(&value));
  EXPECT_EQ(value, 3);  // Should get the latest value
  EXPECT_TRUE(queue_.Empty());  // Queue should be empty after DequeueLatest
}

/**
 * @brief 测试并发生产者-消费者场景
 * 
 * 测试要点：
 * - 验证在高并发下的数据一致性
 * - 测试生产者-消费者模型的正确性
 * - 确保所有数据都被正确处理
 * 
 * 测试参数：
 * - 操作次数：10000
 * - 队列容量：16
 */
TEST_F(BoundedSpscLockfreeQueueTest, ConcurrentProducerConsumer) {
  static constexpr int kNumOperations = 10000;
  ASSERT_TRUE(queue_.Init(16));

  std::atomic<bool> producer_done{false};
  std::vector<int> consumed_values;
  consumed_values.reserve(kNumOperations);

  // Producer thread
  std::thread producer([this]() {
    for (int i = 0; i < kNumOperations; ++i) {
      while (!queue_.Enqueue(i)) {
        std::this_thread::yield();
      }
    }
  });

  // Consumer thread
  std::thread consumer([this, &consumed_values]() {
    int value;
    while (consumed_values.size() < kNumOperations) {
      if (queue_.Dequeue(&value)) {
        consumed_values.push_back(value);
      } else {
        std::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();

  // Verify results
  EXPECT_EQ(consumed_values.size(), kNumOperations);
  for (int i = 0; i < kNumOperations; ++i) {
    EXPECT_EQ(consumed_values[i], i);
  }
}

/**
 * @brief 性能测试用例
 * 
 * 测试场景：
 * 1. 单线程性能测试
 *    - 连续执行100万次入队出队操作
 *    - 测量操作吞吐量
 * 
 * 2. 并发吞吐量测试
 *    - 持续运行5秒
 *    - 测试生产者-消费者模型下的最大吞吐量
 *    - 统计单位时间内的操作次数
 * 
 * 测试参数：
 * - 队列大小：1024
 * - 单线程测试操作数：1,000,000
 * - 并发测试持续时间：5秒
 */
TEST_F(BoundedSpscLockfreeQueueTest, PerformanceTest) {
  static constexpr int kQueueSize = 1024;
  static constexpr int kNumOperations = 1000000;  // 100万次操作
  ASSERT_TRUE(queue_.Init(kQueueSize));

  // 1. Single thread enqueue-dequeue performance
  {
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < kNumOperations; ++i) {
      while (!queue_.Enqueue(i)) {
        std::this_thread::yield();
      }
      
      int value;
      while (!queue_.Dequeue(&value)) {
        std::this_thread::yield();
      }
      EXPECT_EQ(value, i);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double ops_per_sec = static_cast<double>(kNumOperations) / (duration / 1000000.0);
    std::cout << "\nSingle thread performance: " << ops_per_sec << " ops/sec" << std::endl;
  }

  // 2. Concurrent producer-consumer throughput test
  {
    std::atomic<uint64_t> total_ops{0};
    std::atomic<bool> stop{false};
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Producer thread
    std::thread producer([this, &stop, &total_ops]() {
      while (!stop) {
        if (queue_.Enqueue(1)) {
          total_ops.fetch_add(1, std::memory_order_relaxed);
        } else {
          std::this_thread::yield();
        }
      }
    });

    // Consumer thread
    std::thread consumer([this, &stop, &total_ops]() {
      int value;
      while (!stop) {
        if (queue_.Dequeue(&value)) {
          total_ops.fetch_add(1, std::memory_order_relaxed);
        } else {
          std::this_thread::yield();
        }
      }
    });

    // Run for 5 seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));
    stop = true;
    
    producer.join();
    consumer.join();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double ops_per_sec = static_cast<double>(total_ops) / (duration / 1000000.0);
    std::cout << "Concurrent throughput: " << ops_per_sec << " ops/sec" << std::endl;
  }
}

}  // namespace
}  // namespace omnirt::common::util