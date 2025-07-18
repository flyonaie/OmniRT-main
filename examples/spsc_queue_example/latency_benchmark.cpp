/**
 * @file latency_benchmark.cpp
 * @brief 无锁队列延迟性能基准测试
 * 
 * 本文件实现了一个详细的延迟基准测试工具，用于评估BoundedSpscLockfreeQueue
 * 在不同负载条件下的性能表现，包括吞吐量和延迟分布。
 * 
 * 测试包括多种队列大小配置和不同的负载模式，并生成详细的统计报告。
 */

#include <chrono>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <string>
#include <cmath>
#include <mutex>
#include "common/util/bounded_spsc_lockfree_queue.h"

using namespace omnirt::common::util;
using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;

// 测试配置
struct TestConfig {
    std::string name;           // 测试名称
    uint64_t queue_size;        // 队列大小
    bool power_of_two;          // 是否强制使用2的幂大小
    int batch_size;             // 批处理大小
    int num_samples;            // 样本数量
    int producer_delay_us;      // 生产者延迟(微秒)
    int consumer_delay_us;      // 消费者延迟(微秒)
};

/**
 * @brief 测试数据包
 * 
 * 包含时间戳信息用于测量延迟
 */
struct TestData {
    int64_t id;                // 数据ID
    int64_t enqueue_time_ns;   // 入队时间(纳秒)
    int64_t dequeue_time_ns;   // 出队时间(纳秒)
    double payload[10];        // 负载数据(模拟真实数据大小)
    
    TestData() : id(0), enqueue_time_ns(0), dequeue_time_ns(0) {
        for (int i = 0; i < 10; ++i) {
            payload[i] = 0.0;
        }
    }
};

/**
 * @brief 延迟统计结果
 */
struct LatencyStats {
    double min_us;         // 最小延迟(微秒)
    double max_us;         // 最大延迟(微秒)
    double avg_us;         // 平均延迟(微秒)
    double median_us;      // 中位数延迟(微秒)
    double p90_us;         // 90%延迟(微秒)
    double p99_us;         // 99%延迟(微秒)
    double std_dev_us;     // 标准差(微秒)
    double throughput;     // 吞吐量(消息/秒)
    int queue_full_count;  // 队列满计数
};

/**
 * @brief 获取当前时间戳(纳秒)
 * 
 * @return int64_t 当前时间戳
 */
int64_t getNowNs() {
    auto now = Clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
}

/**
 * @brief 计算延迟统计数据
 * 
 * @param latencies 延迟数据数组
 * @param total_time_ms 总测试时间(毫秒)
 * @param queue_full_count 队列满计数
 * @return LatencyStats 计算的统计结果
 */
LatencyStats calculateStats(const std::vector<double>& latencies, double total_time_ms, int queue_full_count) {
    LatencyStats stats;
    stats.queue_full_count = queue_full_count;
    
    if (latencies.empty()) {
        stats.min_us = 0;
        stats.max_us = 0;
        stats.avg_us = 0;
        stats.median_us = 0;
        stats.p90_us = 0;
        stats.p99_us = 0;
        stats.std_dev_us = 0;
        stats.throughput = 0;
        return stats;
    }
    
    // 排序用于计算百分位数
    std::vector<double> sorted_latencies = latencies;
    std::sort(sorted_latencies.begin(), sorted_latencies.end());
    
    // 计算统计值
    stats.min_us = sorted_latencies.front();
    stats.max_us = sorted_latencies.back();
    
    double sum = 0.0;
    for (double latency : sorted_latencies) {
        sum += latency;
    }
    stats.avg_us = sum / sorted_latencies.size();
    
    // 中位数和百分位数
    size_t size = sorted_latencies.size();
    stats.median_us = sorted_latencies[size / 2];
    stats.p90_us = sorted_latencies[static_cast<size_t>(size * 0.9)];
    stats.p99_us = sorted_latencies[static_cast<size_t>(size * 0.99)];
    
    // 标准差
    double variance = 0.0;
    for (double latency : sorted_latencies) {
        double diff = latency - stats.avg_us;
        variance += diff * diff;
    }
    stats.std_dev_us = std::sqrt(variance / size);
    
    // 吞吐量(消息/秒)
    stats.throughput = (sorted_latencies.size() * 1000.0) / total_time_ms;
    
    return stats;
}

/**
 * @brief 打印延迟统计结果
 * 
 * @param config 测试配置
 * @param stats 统计结果
 */
void printStats(const TestConfig& config, const LatencyStats& stats) {
    std::cout << "\n测试 [" << config.name << "] 结果:\n";
    std::cout << "-------------------------------------------------------------\n";
    std::cout << "队列配置:      大小=" << config.queue_size 
              << (config.power_of_two ? " (2的幂)" : "") 
              << ", 批大小=" << config.batch_size << "\n";
    std::cout << "样本数量:      " << config.num_samples << "\n";
    std::cout << "延迟配置:      生产者=" << config.producer_delay_us 
              << "微秒, 消费者=" << config.consumer_delay_us << "微秒\n";
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "最小延迟:      " << stats.min_us << " 微秒\n";
    std::cout << "平均延迟:      " << stats.avg_us << " 微秒\n";
    std::cout << "中位数延迟:    " << stats.median_us << " 微秒\n";
    std::cout << "90%延迟:       " << stats.p90_us << " 微秒\n";
    std::cout << "99%延迟:       " << stats.p99_us << " 微秒\n";
    std::cout << "最大延迟:      " << stats.max_us << " 微秒\n";
    std::cout << "标准差:        " << stats.std_dev_us << " 微秒\n";
    std::cout << "吞吐量:        " << static_cast<int>(stats.throughput) << " 消息/秒\n";
    std::cout << "队列满计数:    " << stats.queue_full_count << "\n";
    std::cout << "-------------------------------------------------------------\n";
}

/**
 * @brief 运行单个基准测试
 * 
 * @param config 测试配置
 * @return LatencyStats 测试结果
 */
LatencyStats runBenchmark(const TestConfig& config) {
    std::cout << "\n开始测试 [" << config.name << "]...\n";
    
    // 创建并初始化队列
    BoundedSpscLockfreeQueue<TestData> queue;
    if (!queue.Init(config.queue_size, config.power_of_two)) {
        std::cerr << "队列初始化失败" << std::endl;
        return LatencyStats();
    }
    
    // 收集的延迟数据
    std::vector<double> latencies;
    latencies.reserve(config.num_samples);
    
    // 控制标志
    std::atomic<bool> producer_done{false};
    std::atomic<bool> consumer_done{false};
    std::atomic<int> queue_full_count{0};
    std::mutex latencies_mutex;
    
    // 记录开始时间
    TimePoint start_time = Clock::now();
    
    // 启动生产者线程
    std::thread producer([&]() {
        for (int i = 0; i < config.num_samples; ++i) {
            TestData data;
            data.id = i;
            data.enqueue_time_ns = getNowNs();
            
            // 填充一些测试数据
            for (int j = 0; j < 10; ++j) {
                data.payload[j] = static_cast<double>(i + j);
            }
            
            // 尝试入队，直到成功
            while (!queue.Enqueue(data)) {
                queue_full_count++;
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
            
            // 模拟生产者工作
            if (config.producer_delay_us > 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(config.producer_delay_us));
            }
            
            // 定期输出进度
            if (i % (config.num_samples / 10) == 0 && i > 0) {
                std::cout << "生产进度: " << (i * 100 / config.num_samples) << "%, 队列大小: " 
                          << queue.Size() << ", 队列满计数: " << queue_full_count << std::endl;
            }
        }
        
        producer_done = true;
    });
    
    // 启动消费者线程
    std::thread consumer([&]() {
        int consumed = 0;
        std::vector<double> local_latencies;
        local_latencies.reserve(config.batch_size);
        
        while (!producer_done || !queue.Empty()) {
            TestData data;
            bool success = false;
            
            // 批量处理以提高效率
            for (int i = 0; i < config.batch_size && (consumed < config.num_samples); ++i) {
                success = queue.Dequeue(&data);
                if (success) {
                    // 记录出队时间
                    data.dequeue_time_ns = getNowNs();
                    
                    // 计算延迟(微秒)
                    double latency_us = (data.dequeue_time_ns - data.enqueue_time_ns) / 1000.0;
                    local_latencies.push_back(latency_us);
                    
                    consumed++;
                } else {
                    // 队列为空，短暂休眠避免CPU空转
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    break;
                }
            }
            
            // 批量添加延迟数据
            if (!local_latencies.empty()) {
                std::lock_guard<std::mutex> lock(latencies_mutex);
                latencies.insert(latencies.end(), local_latencies.begin(), local_latencies.end());
                local_latencies.clear();
            }
            
            // 模拟消费者处理
            if (config.consumer_delay_us > 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(config.consumer_delay_us));
            }
            
            // 定期输出进度
            if (consumed % (config.num_samples / 10) == 0 && consumed > 0) {
                std::cout << "消费进度: " << (consumed * 100 / config.num_samples) 
                          << "%, 队列大小: " << queue.Size() << std::endl;
            }
        }
        
        // 添加最后一批延迟数据
        if (!local_latencies.empty()) {
            std::lock_guard<std::mutex> lock(latencies_mutex);
            latencies.insert(latencies.end(), local_latencies.begin(), local_latencies.end());
        }
        
        consumer_done = true;
    });
    
    // 等待线程完成
    producer.join();
    consumer.join();
    
    // 计算总测试时间
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now() - start_time).count();
        
    std::cout << "测试完成，耗时: " << elapsed << "ms, 收集了 " 
              << latencies.size() << " 个延迟样本" << std::endl;
    
    // 计算并返回统计结果
    return calculateStats(latencies, elapsed, queue_full_count);
}

/**
 * @brief 主函数
 */
int main() {
    std::cout << "BoundedSpscLockfreeQueue 延迟基准测试\n";
    std::cout << "===============================================\n";
    
    // 定义测试配置
    std::vector<TestConfig> configs = {
        // 名称                   队列大小  2的幂  批大小  样本数    生产者延迟  消费者延迟
        {"小队列-均衡负载",        64,      true,  1,      10000,    100,        100},
        {"中队列-均衡负载",        1024,    true,  1,      10000,    100,        100},
        {"大队列-均衡负载",        4096,    true,  1,      10000,    100,        100},
        {"生产者快-消费者慢",      1024,    true,  1,      10000,    10,         200},
        {"生产者慢-消费者快",      1024,    true,  1,      10000,    200,        10},
        {"高吞吐量-批处理",        1024,    true,  10,     100000,   50,         50},
        {"无延迟测试",             1024,    true,  1,      10000,    0,          0},
        {"非2的幂大小测试",        999,     false, 1,      10000,    100,        100}
    };
    
    // 运行所有测试并收集结果
    std::vector<LatencyStats> results;
    
    for (const auto& config : configs) {
        LatencyStats stats = runBenchmark(config);
        results.push_back(stats);
        printStats(config, stats);
    }
    
    // 输出总结报告
    std::cout << "\n总结报告:\n";
    std::cout << "===============================================\n";
    std::cout << std::left << std::setw(20) << "测试名称" 
              << std::right << std::setw(10) << "平均(μs)" 
              << std::setw(10) << "中位数(μs)" 
              << std::setw(10) << "99%(μs)"
              << std::setw(12) << "吞吐量(msg/s)"
              << std::setw(10) << "满计数" << std::endl;
    std::cout << "-----------------------------------------------\n";
    
    for (size_t i = 0; i < configs.size(); ++i) {
        std::cout << std::left << std::setw(20) << configs[i].name.substr(0, 19)
                  << std::fixed << std::setprecision(2)
                  << std::right << std::setw(10) << results[i].avg_us
                  << std::setw(10) << results[i].median_us
                  << std::setw(10) << results[i].p99_us
                  << std::setw(12) << static_cast<int>(results[i].throughput)
                  << std::setw(10) << results[i].queue_full_count << std::endl;
    }
    
    std::cout << "===============================================\n";
    std::cout << "基准测试完成\n";
    
    return 0;
}
