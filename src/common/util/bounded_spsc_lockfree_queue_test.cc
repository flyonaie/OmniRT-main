#include "util/bounded_spsc_lockfree_queue.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>

namespace omnirt::common::util {
namespace {

class BoundedSpscLockfreeQueueTest : public ::testing::Test {
 protected:
  BoundedSpscLockfreeQueue<int> queue_;
};

// Test initialization
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

// Test basic enqueue and dequeue operations
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

// Test queue full behavior
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

// Test DequeueLatest functionality
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

// Test concurrent producer-consumer scenario
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

// Performance test cases
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