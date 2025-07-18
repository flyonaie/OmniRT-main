/**
 * @file perf_test.cpp
 * @brief 共享内存无锁队列性能测试
 * 
 * 此程序测试共享内存无锁队列在不同场景下的性能表现。
 * 测试项目包括：吞吐量、延迟分布、不同大小数据的性能表现等。
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include "common/util/shm_bounded_spsc_lockfree_queue.h"
#include "common/util/bounded_spsc_lockfree_queue.h"

// 用于性能测试的不同大小的数据结构
template<size_t DataSize>
struct PerfTestData {
  int64_t timestamp_ns;  // 发送时间戳（纳秒）
  int32_t seq_id;        // 序列号
  char data[DataSize];   // 有效载荷
  
  PerfTestData() : timestamp_ns(0), seq_id(0) {
    memset(data, 0, DataSize);
  }
};

// 性能测试统计信息
struct PerfStats {
  double total_time_ms;       // 总测试时间（毫秒）
  uint64_t message_count;     // 处理的消息数量
  double msgs_per_sec;        // 每秒消息处理量
  double mb_per_sec;          // 每秒数据处理量（MB）
  double min_latency_us;      // 最小延迟（微秒）
  double max_latency_us;      // 最大延迟（微秒）
  double avg_latency_us;      // 平均延迟（微秒）
  double p50_latency_us;      // 50%延迟（微秒）
  double p90_latency_us;      // 90%延迟（微秒）
  double p99_latency_us;      // 99%延迟（微秒）
  
  PerfStats() 
    : total_time_ms(0), message_count(0), msgs_per_sec(0), mb_per_sec(0),
      min_latency_us(0), max_latency_us(0), avg_latency_us(0),
      p50_latency_us(0), p90_latency_us(0), p99_latency_us(0) {}
};

// 获取当前时间戳（纳秒）
inline int64_t getCurrentTimeNs() {
  auto now = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
      now.time_since_epoch()).count();
}

// 生产者线程函数
template<typename T>
void producerThread(omnirt::common::util::ShmBoundedSpscLockfreeQueue<T>* queue,
                    uint64_t message_count, 
                    std::atomic<bool>* ready,
                    std::atomic<bool>* start,
                    std::atomic<bool>* done) {
  // 等待消费者就绪
  while (!ready->load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  
  // 等待开始信号
  while (!start->load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  
  // 发送消息
  T msg;
  for (uint32_t i = 0; i < message_count; ++i) {
    msg.seq_id = i;
    msg.timestamp_ns = getCurrentTimeNs();
    
    // 持续尝试入队，直到成功
    while (!queue->Enqueue(msg) && !done->load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }
    
    // 如果测试已终止，提前退出
    if (done->load(std::memory_order_acquire)) {
      break;
    }
  }
}

// 消费者线程函数
template<typename T>
void consumerThread(omnirt::common::util::ShmBoundedSpscLockfreeQueue<T>* queue,
                    uint64_t message_count,
                    std::vector<double>* latencies,
                    std::atomic<bool>* ready,
                    std::atomic<bool>* start,
                    std::atomic<bool>* done) {
  // 通知生产者已就绪
  ready->store(true, std::memory_order_release);
  
  // 等待开始信号
  while (!start->load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  
  // 接收消息并记录延迟
  T msg;
  uint64_t received = 0;
  
  auto test_start = std::chrono::high_resolution_clock::now();
  auto last_progress = test_start;
  auto timeout = std::chrono::seconds(5);
  
  while (received < message_count) {
    if (queue->Dequeue(&msg)) {
      int64_t now_ns = getCurrentTimeNs();
      double latency_us = (now_ns - msg.timestamp_ns) / 1000.0;
      latencies->push_back(latency_us);
      received++;
      last_progress = std::chrono::high_resolution_clock::now();
    } else {
      // 检查是否超时（5秒内没有收到新消息）
      auto now = std::chrono::high_resolution_clock::now();
      if (now - last_progress > timeout) {
        std::cout << "消费者超时，只收到 " << received << "/" << message_count << " 条消息" << std::endl;
        break;
      }
      std::this_thread::yield();
    }
  }
  
  // 通知测试结束
  done->store(true, std::memory_order_release);
}

// 计算性能统计
PerfStats calculateStats(const std::vector<double>& latencies, 
                         double total_time_ms,
                         uint64_t message_count,
                         size_t message_size) {
  PerfStats stats;
  stats.total_time_ms = total_time_ms;
  stats.message_count = message_count;
  
  if (latencies.empty()) {
    return stats;
  }
  
  // 复制并排序延迟数据，用于计算百分位数
  std::vector<double> sorted_latencies = latencies;
  std::sort(sorted_latencies.begin(), sorted_latencies.end());
  
  // 计算基本统计量
  stats.min_latency_us = sorted_latencies.front();
  stats.max_latency_us = sorted_latencies.back();
  
  double sum = 0;
  for (double latency : sorted_latencies) {
    sum += latency;
  }
  stats.avg_latency_us = sum / sorted_latencies.size();
  
  // 计算百分位数
  size_t p50_index = sorted_latencies.size() * 0.5;
  size_t p90_index = sorted_latencies.size() * 0.9;
  size_t p99_index = sorted_latencies.size() * 0.99;
  
  stats.p50_latency_us = sorted_latencies[p50_index];
  stats.p90_latency_us = sorted_latencies[p90_index];
  stats.p99_latency_us = sorted_latencies[p99_index];
  
  // 计算吞吐量
  stats.msgs_per_sec = (message_count * 1000.0) / total_time_ms;
  stats.mb_per_sec = (message_count * message_size * 1000.0) / 
                      (total_time_ms * 1024.0 * 1024.0);
  
  return stats;
}

// 打印性能统计结果
void printStats(const std::string& test_name, const PerfStats& stats) {
  std::cout << "====== " << test_name << " 性能测试结果 ======" << std::endl;
  std::cout << "消息数量: " << stats.message_count << std::endl;
  std::cout << "总时间: " << stats.total_time_ms << " ms" << std::endl;
  std::cout << "吞吐量: " << std::fixed << std::setprecision(2) 
            << stats.msgs_per_sec << " 消息/秒, " 
            << stats.mb_per_sec << " MB/秒" << std::endl;
  std::cout << "延迟统计 (微秒):" << std::endl;
  std::cout << "  最小: " << stats.min_latency_us << std::endl;
  std::cout << "  平均: " << stats.avg_latency_us << std::endl;
  std::cout << "  p50: " << stats.p50_latency_us << std::endl;
  std::cout << "  p90: " << stats.p90_latency_us << std::endl;
  std::cout << "  p99: " << stats.p99_latency_us << std::endl;
  std::cout << "  最大: " << stats.max_latency_us << std::endl;
  std::cout << std::endl;
}

// 运行单次性能测试
template<typename T>
PerfStats runPerfTest(const std::string& shm_name, 
                      uint64_t queue_size, 
                      uint64_t message_count,
                      bool is_shared_memory) {
  std::vector<double> latencies;
  latencies.reserve(message_count);
  
  std::atomic<bool> ready(false);
  std::atomic<bool> start(false);
  std::atomic<bool> done(false);
  
  // 创建队列
  omnirt::common::util::ShmBoundedSpscLockfreeQueue<T> queue;
  
  if (!queue.Init(shm_name, queue_size, true, true)) {
    std::cerr << "错误：无法初始化" << (is_shared_memory ? "共享内存" : "") << "队列" << std::endl;
    return PerfStats();
  }
  
  // 创建消费者线程
  std::thread consumer(consumerThread<T>, &queue, message_count, &latencies, 
                      &ready, &start, &done);
  
  // 等待消费者就绪
  while (!ready.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  
  // 创建生产者线程
  std::thread producer(producerThread<T>, &queue, message_count, 
                      &ready, &start, &done);
  
  // 开始测试计时
  auto test_start = std::chrono::high_resolution_clock::now();
  start.store(true, std::memory_order_release);
  
  // 等待线程完成
  producer.join();
  consumer.join();
  
  // 结束测试计时
  auto test_end = std::chrono::high_resolution_clock::now();
  double total_time_ms = std::chrono::duration<double, std::milli>(
      test_end - test_start).count();
  
  // 计算并返回统计结果
  return calculateStats(latencies, total_time_ms, latencies.size(), sizeof(T));
}

int main(int argc, char* argv[]) {
  // 解析命令行参数
  uint64_t queue_size = 1024;
  uint64_t message_count = 1000000;
  bool run_small_test = true;
  bool run_medium_test = true;
  bool run_large_test = true;
  
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--queue-size" && i + 1 < argc) {
      queue_size = std::stoull(argv[++i]);
    } else if (arg == "--messages" && i + 1 < argc) {
      message_count = std::stoull(argv[++i]);
    } else if (arg == "--small-only") {
      run_medium_test = false;
      run_large_test = false;
    } else if (arg == "--medium-only") {
      run_small_test = false;
      run_large_test = false;
    } else if (arg == "--large-only") {
      run_small_test = false;
      run_medium_test = false;
    } else if (arg == "--help") {
      std::cout << "用法: " << argv[0] << " [选项]" << std::endl
                << "选项:" << std::endl
                << "  --queue-size SIZE   队列大小 (默认: 1024)" << std::endl
                << "  --messages COUNT    测试消息数量 (默认: 1000000)" << std::endl
                << "  --small-only        只运行小型数据测试" << std::endl
                << "  --medium-only       只运行中型数据测试" << std::endl
                << "  --large-only        只运行大型数据测试" << std::endl
                << "  --help              显示此帮助信息" << std::endl;
      return 0;
    }
  }
  
  std::cout << "共享内存无锁队列性能测试" << std::endl;
  std::cout << "队列大小: " << queue_size << ", 消息数量: " << message_count << std::endl;
  std::cout << std::endl;
  
  // 运行不同大小数据的测试
  if (run_small_test) {
    // 小型数据测试：64字节有效载荷
    using SmallData = PerfTestData<64>;
    auto small_stats = runPerfTest<SmallData>(
        "/perf_test_small", queue_size, message_count, true);
    printStats("小型数据(64字节)", small_stats);
  }
  
  if (run_medium_test) {
    // 中型数据测试：512字节有效载荷
    using MediumData = PerfTestData<512>;
    auto medium_stats = runPerfTest<MediumData>(
        "/perf_test_medium", queue_size, message_count / 2, true);
    printStats("中型数据(512字节)", medium_stats);
  }
  
  if (run_large_test) {
    // 大型数据测试：4KB有效载荷
    using LargeData = PerfTestData<4096>;
    auto large_stats = runPerfTest<LargeData>(
        "/perf_test_large", queue_size, message_count / 10, true);
    printStats("大型数据(4KB)", large_stats);
  }
  
  std::cout << "性能测试完成" << std::endl;
  return 0;
}
