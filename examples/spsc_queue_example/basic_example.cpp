/**
 * @file basic_example.cpp
 * @brief 单生产者-单消费者无锁队列基础示例
 * 
 * 本示例展示了如何基本使用BoundedSpscLockfreeQueue进行线程间通信。
 * 示例包含一个生产者线程和一个消费者线程，它们通过无锁队列传递数据。
 */

#include <chrono>
#include <iostream>
#include <thread>
#include <atomic>
#include "common/util/bounded_spsc_lockfree_queue.h"

using namespace omnirt::common::util;

// 测试参数
constexpr int QUEUE_SIZE = 1024;           // 队列容量
constexpr int NUM_ITEMS = 100000;          // 要处理的项目数量
constexpr int PRODUCER_SLEEP_US = 100;     // 生产者休眠时间(微秒)
constexpr int CONSUMER_SLEEP_US = 200;     // 消费者休眠时间(微秒)

/**
 * @brief 主函数
 */
int main() {
    // 创建并初始化队列(使用2的幂大小以优化索引计算)
    BoundedSpscLockfreeQueue<int> queue;
    if (!queue.Init(QUEUE_SIZE, true)) {
        std::cerr << "队列初始化失败" << std::endl;
        return 1;
    }
    
    std::cout << "初始化成功，队列容量: " << queue.Capacity() << std::endl;
    
    // 用于标记生产者完成
    std::atomic<bool> producer_done{false};
    // 记录生产和消费的项目数
    std::atomic<int> produced_count{0};
    std::atomic<int> consumed_count{0};
    
    // 启动生产者线程
    std::thread producer([&]() {
        std::cout << "生产者启动" << std::endl;
        
        for (int i = 0; i < NUM_ITEMS; ++i) {
            // 尝试入队直到成功
            while (!queue.Enqueue(i)) {
                // 队列已满，短暂休眠避免CPU空转
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
            
            produced_count++;
            
            // 模拟生产者工作
            std::this_thread::sleep_for(std::chrono::microseconds(PRODUCER_SLEEP_US));
            
            // 定期输出状态
            if (i % 10000 == 0 && i > 0) {
                std::cout << "已生产: " << i << " 项, 队列大小: " << queue.Size() << std::endl;
            }
        }
        
        producer_done = true;
        std::cout << "生产者完成，共生产 " << produced_count << " 项" << std::endl;
    });
    
    // 启动消费者线程
    std::thread consumer([&]() {
        std::cout << "消费者启动" << std::endl;
        
        int value;
        // 持续消费，直到生产者完成且队列空
        while (!producer_done || !queue.Empty()) {
            if (queue.Dequeue(&value)) {
                consumed_count++;
                
                // 定期输出状态
                if (consumed_count % 10000 == 0) {
                    std::cout << "已消费: " << consumed_count << " 项, 最新值: " << value 
                              << ", 队列大小: " << queue.Size() << std::endl;
                }
                
                // 模拟消费者处理
                std::this_thread::sleep_for(std::chrono::microseconds(CONSUMER_SLEEP_US));
            } else {
                // 队列为空，短暂休眠避免CPU空转
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
        
        std::cout << "消费者完成，共消费 " << consumed_count << " 项" << std::endl;
    });
    
    // 等待线程完成
    producer.join();
    consumer.join();
    
    // 验证结果
    if (produced_count == consumed_count && consumed_count == NUM_ITEMS) {
        std::cout << "测试成功! 所有 " << NUM_ITEMS << " 个项目均正确传输" << std::endl;
    } else {
        std::cout << "测试失败! 生产: " << produced_count 
                  << ", 消费: " << consumed_count 
                  << ", 期望: " << NUM_ITEMS << std::endl;
    }
    
    return 0;
}
