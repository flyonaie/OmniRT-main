/**
 * @file producer.cpp
 * @brief 共享内存无锁队列生产者示例
 * 
 * 该示例展示了如何作为生产者进程使用共享内存无锁队列。
 * 程序会创建一个共享内存队列并向其中写入数据，可以和consumer.cpp一起测试。
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <signal.h>
#include <atomic>
#include <cstring>
#include <sys/mman.h>
#include <fcntl.h>
#include "common/util/shm_bounded_spsc_lockfree_queue.h"

// 消息结构体
struct Message {
  int id;
  double timestamp;
  char data[128];

  Message() : id(0), timestamp(0.0) {
    memset(data, 0, sizeof(data));
  }

  Message(int i, const std::string& msg) : id(i) {
    timestamp = std::chrono::duration_cast<std::chrono::duration<double>>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    strncpy(data, msg.c_str(), sizeof(data) - 1);
    data[sizeof(data) - 1] = '\0';
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
  int interval_ms = 500;
  
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--name" && i + 1 < argc) {
      shm_name = argv[++i];
      if (shm_name[0] != '/') shm_name = "/" + shm_name;
    } else if (arg == "--size" && i + 1 < argc) {
      queue_size = std::stoi(argv[++i]);
    } else if (arg == "--interval" && i + 1 < argc) {
      interval_ms = std::stoi(argv[++i]);
    } else if (arg == "--help") {
      std::cout << "用法: " << argv[0] << " [选项]" << std::endl
                << "选项:" << std::endl
                << "  --name NAME       共享内存名称 (默认: /shm_queue_example)" << std::endl
                << "  --size SIZE       队列大小 (默认: 100)" << std::endl
                << "  --interval MS     消息发送间隔，毫秒 (默认: 500)" << std::endl
                << "  --help            显示此帮助信息" << std::endl;
      return 0;
    }
  }

  std::cout << "启动生产者程序，共享内存名称: " << shm_name << ", 队列大小: " 
            << queue_size << ", 间隔: " << interval_ms << "ms" << std::endl;

  // 创建共享内存队列
  std::cout << "DEBUG: 开始初始化队列..." << std::endl;
  
  // 首先清理可能存在的同名共享内存
  std::cout << "DEBUG: 尝试删除已存在的共享内存: " << shm_name << std::endl;
  if (shm_unlink(shm_name.c_str()) == 0) {
    std::cout << "DEBUG: 成功删除已存在的共享内存" << std::endl;
  } else {
    if (errno == ENOENT) {
      std::cout << "DEBUG: 没有找到要删除的共享内存，可能不存在" << std::endl;
    } else {
      std::cerr << "DEBUG: 删除共享内存失败: " << strerror(errno) << " (errno=" << errno << ")" << std::endl;
      perror("shm_unlink");
    }
  }
  
  // 查看共享内存目录
  std::cout << "DEBUG: 检查/dev/shm目录内容:" << std::endl;
  system("ls -la /dev/shm");

  // 初始化队列
  omnirt::common::util::ShmBoundedSpscLockfreeQueue<Message> queue;
  
  // 使用true指定我们是创建者
  if (!queue.Init(shm_name, queue_size, true, true)) {
    std::cerr << "错误：无法初始化共享内存队列" << std::endl;
    return 1;
  }
  
  std::cout << "DEBUG: 队列初始化成功" << std::endl;

  std::cout << "队列已初始化，等待消费者连接..." << std::endl;
  std::cout << "按Ctrl+C终止程序" << std::endl;

  // 生产消息
  int messageId = 0;
  
  while (g_running) {
    std::string message = "消息 #" + std::to_string(messageId) + " 发送于 " + 
                         std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    Message msg(messageId, message);
    
    // 尝试入队
    if (queue.Enqueue(msg)) {
      std::cout << "已发送: [" << messageId << "] " << msg.data << std::endl;
    } else {
      std::cout << "队列已满，消息 #" << messageId << " 未发送" << std::endl;
    }
    
    // 休眠
    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    messageId++;
  }

  std::cout << "生产者正在清理资源..." << std::endl;
  // 资源会在析构函数中自动清理
  return 0;
}
