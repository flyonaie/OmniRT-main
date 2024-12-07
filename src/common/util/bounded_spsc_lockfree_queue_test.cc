// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include "util/bounded_spsc_lockfree_queue.h"

namespace omnirt::common::util {

TEST(BoundedSpscLockfreeQueueTest, BasicEnqueueDequeue) {
  BoundedSpscLockfreeQueue<int> queue;
  queue.Init(4);  // 使用一个小的队列大小来测试边界情况
  
  ASSERT_TRUE(queue.Empty());
  ASSERT_TRUE(queue.Enqueue(1));
  ASSERT_EQ(queue.Size(), 1);
  
  int value;
  ASSERT_TRUE(queue.Dequeue(&value));
  ASSERT_EQ(value, 1);
  ASSERT_TRUE(queue.Empty());
}

TEST(BoundedSpscLockfreeQueueTest, QueueFull) {
  BoundedSpscLockfreeQueue<int> queue;
  queue.Init(4);
  
  // 填满队列
  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(queue.Enqueue(i));
  }
  
  // 队列已满，应该无法继续入队
  ASSERT_FALSE(queue.Enqueue(4));
}

TEST(BoundedSpscLockfreeQueueTest, MoveSemantics) {
  std::string str = "test";
  BoundedSpscLockfreeQueue<std::string> str_queue;
  str_queue.Init(4);
  
  ASSERT_TRUE(str_queue.Enqueue(std::move(str)));
  ASSERT_TRUE(str.empty());  // 原字符串应该被移动
  
  std::string out_str;
  ASSERT_TRUE(str_queue.Dequeue(&out_str));
  ASSERT_EQ(out_str, "test");
}

TEST(BoundedSpscLockfreeQueueTest, ProducerConsumer) {
  BoundedSpscLockfreeQueue<int> queue;
  queue.Init(100);  // 使用较大的队列大小避免频繁阻塞
  
  const int num_items = 1000;
  std::atomic<int> consumed_sum{0};
  std::atomic<bool> done{false};
  
  // 消费者线程
  std::thread consumer([&]() {
    int value;
    while (!done || !queue.Empty()) {
      if (queue.Dequeue(&value)) {
        // 消费一个元素
        std::cout << "Consumed: " << value << std::endl;
        consumed_sum += value;
      }
    }
  });

  // 生产者线程（当前线程）
  int expected_sum = 0;
  for (int i = 0; i < num_items; ++i) {
    while (!queue.Enqueue(i)) {
      // 队列满，等待一下
      std::this_thread::yield();
    }
    std::cout << "Produced: " << i << std::endl;
    expected_sum += i;
  }
  
  done = true;
  consumer.join();
  
  ASSERT_EQ(consumed_sum, expected_sum);
}

TEST(BoundedSpscLockfreeQueueTest, SizeCalculation) {
  BoundedSpscLockfreeQueue<int> queue;
  queue.Init(4);
  
  ASSERT_EQ(queue.Size(), 0);
  
  queue.Enqueue(1);
  ASSERT_EQ(queue.Size(), 1);
  
  queue.Enqueue(2);
  ASSERT_EQ(queue.Size(), 2);
  
  int value;
  queue.Dequeue(&value);
  ASSERT_EQ(queue.Size(), 1);
  ASSERT_EQ(value, 1);
  
  queue.Dequeue(&value);
  ASSERT_EQ(queue.Size(), 0);
  ASSERT_EQ(value, 2);
}

TEST(BoundedSpscLockfreeQueueTest, PowerOfTwoSize) {
  BoundedSpscLockfreeQueue<int> queue;
  // 请求大小3，应该得到4
  ASSERT_TRUE(queue.Init(3));
  
  // 应该能存储4个元素
  ASSERT_TRUE(queue.Enqueue(1));
  ASSERT_TRUE(queue.Enqueue(2));
  ASSERT_TRUE(queue.Enqueue(3));
  ASSERT_TRUE(queue.Enqueue(4));
  ASSERT_FALSE(queue.Enqueue(5));  // 第5个应该失败
}

TEST(BoundedSpscLockfreeQueueTest, TailElementAndGetElement) {
  BoundedSpscLockfreeQueue<int> queue;
  queue.Init(4);
  
  // 测试空队列
  int value;
  ASSERT_FALSE(queue.TailElement(&value));
  ASSERT_FALSE(queue.TailElement(nullptr));
  
  // 添加元素后测试
  queue.Enqueue(42);
  ASSERT_TRUE(queue.TailElement(&value));
  ASSERT_EQ(value, 42);
  ASSERT_FALSE(queue.TailElement(nullptr));
  
  // 测试 GetElement
  ASSERT_TRUE(queue.GetElement(0, &value));
  ASSERT_EQ(value, 42);
  ASSERT_FALSE(queue.GetElement(0, nullptr));
}

TEST(BoundedSpscLockfreeQueueTest, NullPointerHandling) {
  BoundedSpscLockfreeQueue<int> queue;
  queue.Init(4);
  
  // 测试 Enqueue 后的 Dequeue null
  queue.Enqueue(1);
  ASSERT_FALSE(queue.Dequeue(nullptr));
  ASSERT_EQ(queue.Size(), 1);  // 元素应该还在队列中
  
  // 正常 Dequeue
  int value;
  ASSERT_TRUE(queue.Dequeue(&value));
  ASSERT_EQ(value, 1);
}

TEST(BoundedSpscLockfreeQueueTest, PerformanceTest) {
  BoundedSpscLockfreeQueue<int64_t> queue;
  constexpr size_t QUEUE_SIZE = 128;  // 减小队列大小
  ASSERT_TRUE(queue.Init(QUEUE_SIZE)) << "Failed to initialize queue";
  
  // 减小测试规模
  constexpr size_t NUM_OPS = 1000;  // 1000次操作
  std::atomic<bool> start{false};
  std::atomic<bool> done{false};
  std::atomic<size_t> remaining_to_consume{NUM_OPS};
  
  // 统计数据
  std::atomic<size_t> total_enqueued{0};
  std::atomic<size_t> total_dequeued{0};
  std::atomic<int64_t> total_latency{0};
  
  // 消费者线程
  std::thread consumer([&]() {
    int64_t value;
    size_t local_dequeued = 0;
    size_t fail_count = 0;
    
    while (!start) {
      std::this_thread::yield();
    }
    
    std::cout << "Consumer started" << std::endl;
    
    while (remaining_to_consume > 0) {
      if (queue.Dequeue(&value)) {
        int64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        total_latency.fetch_add(now - value);
        local_dequeued++;
        total_dequeued.fetch_add(1);
        remaining_to_consume--;
        
        if (local_dequeued % 100 == 0) {  // 更频繁的输出
          std::cout << "Dequeued: " << local_dequeued 
                    << ", Remaining: " << remaining_to_consume
                    << ", Queue size: " << queue.Size() << std::endl;
        }
      } else {
        fail_count++;
        if (fail_count % 10000 == 0) {  // 减少失败输出频率
          std::cout << "Dequeue failed " << fail_count 
                    << " times, Remaining: " << remaining_to_consume
                    << ", Queue size: " << queue.Size() << std::endl;
        }
        std::this_thread::yield();
      }
    }
    std::cout << "Consumer finished. Total dequeued: " << local_dequeued << std::endl;
  });
  
  // 生产者线程
  size_t enqueued = 0;
  size_t fail_count = 0;
  
  // 预热
  const size_t warmup_size = std::min(QUEUE_SIZE/4, static_cast<size_t>(32));
  std::cout << "Starting warmup with " << warmup_size << " items" << std::endl;
  
  for (size_t i = 0; i < warmup_size && queue.Enqueue(0); ++i) {
    std::cout << "Warmup enqueued: " << i + 1 << std::endl;
  }
  
  std::cout << "Warmup complete. Starting test" << std::endl;
  auto start_time = std::chrono::high_resolution_clock::now();
  start = true;
  
  while (enqueued < NUM_OPS) {
    int64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    if (queue.Enqueue(now)) {
      enqueued++;
      total_enqueued.fetch_add(1);
      
      if (enqueued % 100 == 0) {
        std::cout << "Enqueued: " << enqueued 
                  << ", Queue size: " << queue.Size() << std::endl;
      }
    } else {
      fail_count++;
      if (fail_count % 10000 == 0) {
        std::cout << "Enqueue failed " << fail_count 
                  << " times, Queue size: " << queue.Size() << std::endl;
      }
      std::this_thread::yield();
    }
  }
  
  std::cout << "Producer finished. Total enqueued: " << enqueued << std::endl;
  done = true;
  
  // 等待消费者消费完所有数据
  while (remaining_to_consume > 0) {
    std::cout << "Waiting for consumer, remaining: " << remaining_to_consume << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  
  consumer.join();
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
  
  // 输出性能统计
  double seconds = duration.count() / 1000000.0;
  double ops_per_sec = NUM_OPS / seconds;
  double avg_latency_ns = static_cast<double>(total_latency) / total_dequeued;
  
  std::cout << "\nPerformance Test Results:" << std::endl;
  std::cout << "Total time: " << seconds << " seconds" << std::endl;
  std::cout << "Operations/sec: " << ops_per_sec << std::endl;
  std::cout << "Average latency: " << avg_latency_ns << " ns" << std::endl;
  std::cout << "Total enqueued: " << total_enqueued << std::endl;
  std::cout << "Total dequeued: " << total_dequeued << std::endl;
  std::cout << "Enqueue failed: " << fail_count << " times" << std::endl;
  std::cout << "Final queue size: " << queue.Size() << std::endl;
  
  // 验证结果
  EXPECT_EQ(total_enqueued.load(), NUM_OPS) << "Enqueued count mismatch";
  EXPECT_EQ(total_dequeued.load(), NUM_OPS) << "Dequeued count mismatch";
  EXPECT_EQ(remaining_to_consume.load(), 0) << "Not all items were consumed";
  EXPECT_EQ(queue.Size(), 0) << "Queue should be empty at end";
}

}  // namespace omnirt::common::util