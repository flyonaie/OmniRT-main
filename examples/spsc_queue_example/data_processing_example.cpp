/**
 * @file data_processing_example.cpp
 * @brief 基于无锁队列的数据处理流水线示例
 * 
 * 本示例演示了使用BoundedSpscLockfreeQueue构建实时数据处理流水线的场景。
 * 生产者模拟传感器数据源，定期采集数据；消费者对数据进行处理和分析。
 * 
 * 此示例特别针对机器人开发中常见的传感器数据处理场景进行了优化。
 */

#include <chrono>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include "common/util/bounded_spsc_lockfree_queue.h"

using namespace omnirt::common::util;
using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;

/**
 * @brief 传感器数据包结构
 * 
 * 模拟机器人中传感器采集的数据包
 */
struct SensorData {
    int64_t timestamp;    ///< 时间戳(纳秒)
    int32_t sensor_id;    ///< 传感器ID
    double values[6];     ///< 测量值(例如IMU数据: 加速度x/y/z + 角速度x/y/z)
    
    // 默认构造函数(用于队列初始化)
    SensorData() : timestamp(0), sensor_id(0) {
        for (int i = 0; i < 6; ++i) {
            values[i] = 0.0;
        }
    }
};

/**
 * @brief 处理后的数据结构
 * 
 * 存储处理后的传感器数据分析结果
 */
struct ProcessedData {
    int64_t source_timestamp;       ///< 原始数据时间戳
    int64_t processing_timestamp;   ///< 处理时间戳
    int32_t sensor_id;              ///< 传感器ID
    double magnitude;               ///< 计算的信号强度
    double latency_ms;              ///< 处理延迟(毫秒)
    
    // 默认构造函数(用于队列初始化)
    ProcessedData() : source_timestamp(0), processing_timestamp(0), 
                      sensor_id(0), magnitude(0.0), latency_ms(0.0) {}
};

// 测试参数
constexpr int QUEUE_SIZE = 1024;           // 队列容量
constexpr int NUM_SENSORS = 3;             // 传感器数量
constexpr int TOTAL_SAMPLES = 10000;       // 总样本数
constexpr int SENSOR_INTERVAL_US = 1000;   // 传感器采样间隔(微秒)

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
 * @brief 主函数
 */
int main() {
    // 创建数据队列和结果队列
    BoundedSpscLockfreeQueue<SensorData> data_queue;
    BoundedSpscLockfreeQueue<ProcessedData> result_queue;
    
    if (!data_queue.Init(QUEUE_SIZE, true) || !result_queue.Init(QUEUE_SIZE, true)) {
        std::cerr << "队列初始化失败" << std::endl;
        return 1;
    }
    
    std::cout << "初始化成功，数据队列容量: " << data_queue.Capacity() 
              << ", 结果队列容量: " << result_queue.Capacity() << std::endl;
    
    // 统计信息
    std::atomic<int> samples_produced{0};
    std::atomic<int> samples_processed{0};
    std::atomic<int> samples_dropped{0};
    std::atomic<bool> producer_done{false};
    std::atomic<bool> processor_done{false};
    
    // 延迟统计
    std::vector<double> latencies;
    latencies.reserve(TOTAL_SAMPLES);
    
    // 启动数据生产者线程(模拟多个传感器)
    std::thread producer([&]() {
        std::cout << "传感器数据生产者启动，模拟 " << NUM_SENSORS << " 个传感器" << std::endl;
        
        TimePoint start_time = Clock::now();
        int samples_per_sensor = TOTAL_SAMPLES / NUM_SENSORS;
        
        for (int i = 0; i < samples_per_sensor; ++i) {
            for (int sensor_id = 0; sensor_id < NUM_SENSORS; ++sensor_id) {
                // 创建新的传感器数据
                SensorData data;
                data.timestamp = getNowNs();
                data.sensor_id = sensor_id;
                
                // 模拟传感器数据(简单的正弦波)
                double time_sec = i * 0.001; // 以秒为单位的时间
                data.values[0] = std::sin(2 * M_PI * 1.0 * time_sec) * (sensor_id + 1);  // 加速度X
                data.values[1] = std::sin(2 * M_PI * 1.2 * time_sec) * (sensor_id + 1);  // 加速度Y
                data.values[2] = std::sin(2 * M_PI * 1.5 * time_sec) * (sensor_id + 1);  // 加速度Z
                data.values[3] = std::sin(2 * M_PI * 0.8 * time_sec) * (sensor_id + 1);  // 角速度X
                data.values[4] = std::sin(2 * M_PI * 0.9 * time_sec) * (sensor_id + 1);  // 角速度Y
                data.values[5] = std::sin(2 * M_PI * 1.1 * time_sec) * (sensor_id + 1);  // 角速度Z
                
                // 尝试入队，如果队列满则丢弃并统计
                if (!data_queue.Enqueue(data)) {
                    samples_dropped++;
                } else {
                    samples_produced++;
                }
            }
            
            // 按照传感器采样间隔模拟数据生成
            std::this_thread::sleep_for(std::chrono::microseconds(SENSOR_INTERVAL_US));
            
            // 定期输出状态
            if (i % 1000 == 0 && i > 0) {
                int produced = samples_produced.load();
                int dropped = samples_dropped.load();
                std::cout << "已生成: " << produced << " 样本, 丢弃: " << dropped 
                          << ", 队列大小: " << data_queue.Size() << std::endl;
            }
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - start_time).count();
            
        producer_done = true;
        std::cout << "传感器模拟完成，耗时: " << elapsed << "ms, 产生样本: " 
                  << samples_produced << ", 丢弃: " << samples_dropped << std::endl;
    });
    
    // 启动数据处理线程
    std::thread processor([&]() {
        std::cout << "数据处理线程启动" << std::endl;
        
        SensorData data;
        
        while (!producer_done || !data_queue.Empty()) {
            // 尝试获取数据
            if (data_queue.Dequeue(&data)) {
                // 创建处理结果
                ProcessedData result;
                result.source_timestamp = data.timestamp;
                result.processing_timestamp = getNowNs();
                result.sensor_id = data.sensor_id;
                
                // 计算信号强度(简单示例，计算六轴数据的平方和开方)
                double sum_squares = 0.0;
                for (int i = 0; i < 6; ++i) {
                    sum_squares += data.values[i] * data.values[i];
                }
                result.magnitude = std::sqrt(sum_squares);
                
                // 计算处理延迟(毫秒)
                result.latency_ms = (result.processing_timestamp - result.source_timestamp) / 1e6;
                
                // 将结果放入结果队列
                if (!result_queue.Enqueue(result)) {
                    std::cerr << "警告: 结果队列已满，结果丢失" << std::endl;
                } else {
                    samples_processed++;
                }
                
                // 模拟一些处理时间
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            } else {
                // 队列为空，短暂休眠避免CPU空转
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
        
        processor_done = true;
        std::cout << "数据处理完成，共处理 " << samples_processed << " 样本" << std::endl;
    });
    
    // 启动结果消费者线程
    std::thread consumer([&]() {
        std::cout << "结果消费者线程启动" << std::endl;
        
        ProcessedData result;
        int results_consumed = 0;
        
        while (!processor_done || !result_queue.Empty()) {
            if (result_queue.Dequeue(&result)) {
                results_consumed++;
                
                // 记录延迟数据用于统计
                latencies.push_back(result.latency_ms);
                
                // 定期输出一些结果
                if (results_consumed % 1000 == 0) {
                    std::cout << "处理了 " << results_consumed 
                              << " 个结果，传感器 " << result.sensor_id 
                              << " 的信号强度: " << result.magnitude 
                              << ", 延迟: " << result.latency_ms << "ms" << std::endl;
                }
                
                // 模拟结果处理时间
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            } else {
                // 队列为空，短暂休眠避免CPU空转
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
        
        std::cout << "结果消费完成，共处理 " << results_consumed << " 个结果" << std::endl;
    });
    
    // 等待所有线程完成
    producer.join();
    processor.join();
    consumer.join();
    
    // 计算延迟统计
    if (!latencies.empty()) {
        // 计算平均延迟
        double sum = 0.0;
        for (double latency : latencies) {
            sum += latency;
        }
        double avg_latency = sum / latencies.size();
        
        // 排序以计算百分位数
        std::sort(latencies.begin(), latencies.end());
        
        double min_latency = latencies.front();
        double median_latency = latencies[latencies.size() / 2];
        double p99_latency = latencies[static_cast<size_t>(latencies.size() * 0.99)];
        double max_latency = latencies.back();
        
        // 输出统计结果
        std::cout << "\n性能统计:\n";
        std::cout << "------------------------------\n";
        std::cout << "样本数量:       " << latencies.size() << "\n";
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "最小延迟:       " << min_latency << " ms\n";
        std::cout << "平均延迟:       " << avg_latency << " ms\n";
        std::cout << "中位数延迟:     " << median_latency << " ms\n";
        std::cout << "99%延迟:        " << p99_latency << " ms\n";
        std::cout << "最大延迟:       " << max_latency << " ms\n";
        std::cout << "------------------------------\n";
    }
    
    // 输出最终统计
    std::cout << "\n总统计:\n";
    std::cout << "生产的样本:       " << samples_produced << "\n";
    std::cout << "处理的样本:       " << samples_processed << "\n";
    std::cout << "丢弃的样本:       " << samples_dropped << "\n";
    
    return 0;
}
