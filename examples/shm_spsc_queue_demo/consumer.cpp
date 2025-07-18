/**
 * @file consumer.cpp
 * @brief 共享内存无锁队列消费者示例
 * 
 * 该示例展示了如何作为消费者进程使用共享内存无锁队列。
 * 程序会连接到已创建的共享内存队列并从中读取数据，需要和producer.cpp一起测试。
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <signal.h>
#include <atomic>
#include <cstring>
#include <iomanip>
#include "common/util/shm_bounded_spsc_lockfree_queue.h"

// 消息结构体 - 必须与生产者定义完全相同
struct Message {
  int id;
  double timestamp;
  char data[128];

  Message() : id(0), timestamp(0.0) {
    memset(data, 0, sizeof(data));
  }
};

// 全局变量
std::atomic<bool> g_running{true};

// 信号处理函数
void signalHandler(int signum) {
  std::cout << "中断信号 (" << signum << ") 已收到，准备退出..." << std::endl;
  g_running = false;
}

int main(int argc, char* argv[]) {
  // 处理中断信号
  signal(SIGINT, signalHandler);
  
  // 解析命令行参数
  std::string shm_name = "/shm_queue_example";
  size_t queue_size = 128;
  bool latest_only = false;
  
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--name" && i + 1 < argc) {
      shm_name = argv[++i];
      if (shm_name[0] != '/') shm_name = "/" + shm_name;
    } else if (arg == "--size" && i + 1 < argc) {
      queue_size = std::stoi(argv[++i]);
    } else if (arg == "--latest") {
      latest_only = true;
    } else if (arg == "--help") {
      std::cout << "用法: " << argv[0] << " [选项]" << std::endl
                << "选项:" << std::endl
                << "  --name NAME    共享内存名称 (默认: /shm_queue_example)" << std::endl
                << "  --size SIZE    队列大小 (默认: 100)" << std::endl
                << "  --latest       只获取最新的消息 (跳过旧消息)" << std::endl
                << "  --help         显示此帮助信息" << std::endl;
      return 0;
    }
  }

  std::cout << "启动消费者程序，共享内存名称: " << shm_name << ", 队列大小: " 
            << queue_size << (latest_only ? ", 只获取最新消息" : "") << std::endl;

  // 创建共享内存队列
  omnirt::common::util::ShmBoundedSpscLockfreeQueue<Message> queue;
  
  if (!queue.Init(shm_name, queue_size, true, false)) {
    std::cerr << "错误：无法连接到共享内存队列，请确保先运行生产者" << std::endl;
    return 1;
  }

  std::cout << "已连接到共享内存队列" << std::endl;
  std::cout << "按Ctrl+C终止程序" << std::endl;

  // 消费消息
  Message msg;
  uint64_t received_count = 0;
  uint64_t last_id = -1;
  
  while (g_running) {
    bool got_message = false;
    
    // 根据模式选择获取消息的方式
    if (latest_only) {
      got_message = queue.DequeueLatest(&msg);
    } else {
      got_message = queue.Dequeue(&msg);
    }
    
    // 处理消息
    if (got_message) {
      received_count++;
      
      // 计算延迟
      double now = std::chrono::duration_cast<std::chrono::duration<double>>(
          std::chrono::high_resolution_clock::now().time_since_epoch()).count();
      double latency_ms = (now - msg.timestamp) * 1000.0;
      
      // 检测丢失的消息
      std::string lost_info;
      if (last_id != static_cast<uint64_t>(-1) && last_id + 1 != static_cast<uint64_t>(msg.id)) {
        lost_info = " [丢失了 " + std::to_string(msg.id - last_id - 1) + " 条消息]";
      }
      last_id = msg.id;
      
      std::cout << "已接收: [" << msg.id << "] " << msg.data 
                << " 延迟: " << std::fixed << std::setprecision(3) << latency_ms << " ms"
                << lost_info << std::endl;
    } else {
      // 队列为空时短暂休眠，避免CPU空转
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  std::cout << "消费者正在清理资源... 总共接收 " << received_count << " 条消息" << std::endl;
  // 资源会在析构函数中自动清理
  return 0;
}
