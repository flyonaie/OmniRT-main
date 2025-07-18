/**
 * @file bidirectional_demo.cpp
 * @brief 双向共享内存通信示例
 * 
 * 此示例演示如何使用两个共享内存队列构建双向通信通道。
 * 程序使用多线程模拟两个进程之间的通信，一个线程作为命令发送方，
 * 另一个线程作为数据发送方，展示完整的请求-响应模式。
 */

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <cstring>
#include <signal.h>
#include <cstdlib>
#include <mutex>
#include "common/util/shm_bounded_spsc_lockfree_queue.h"

// 全局变量，控制程序终止
std::atomic<bool> g_running{true};
std::mutex g_console_mutex;

// 信号处理函数
void signalHandler(int signum) {
  {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::cout << "中断信号 (" << signum << ") 已收到，准备退出..." << std::endl;
  }
  g_running = false;
}

// 命令消息结构
struct CommandMessage {
  enum CommandType {
    GET_DATA = 1,
    SET_PARAM = 2,
    SHUTDOWN = 3
  };
  
  int cmd_id;
  CommandType type;
  double param_value;
  char description[64];
  
  CommandMessage() : cmd_id(0), type(GET_DATA), param_value(0.0) {
    memset(description, 0, sizeof(description));
  }
};

// 数据消息结构
struct DataMessage {
  int response_to_cmd_id;
  bool success;
  double value[10];
  char message[128];
  
  DataMessage() : response_to_cmd_id(0), success(false) {
    memset(value, 0, sizeof(value));
    memset(message, 0, sizeof(message));
  }
};

// 命令处理线程函数
void dataProviderThread() {
  // 命令接收队列（命令发送方->数据提供方）
  omnirt::common::util::ShmBoundedSpscLockfreeQueue<CommandMessage> cmd_queue;
  
  // 数据发送队列（数据提供方->命令发送方）
  omnirt::common::util::ShmBoundedSpscLockfreeQueue<DataMessage> data_queue;
  
  // 初始化队列
  if (!cmd_queue.Init("/bidirectional_cmd_queue", 50, true, false)) {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::cerr << "[数据提供方] 错误：无法连接到命令队列" << std::endl;
    g_running = false;
    return;
  }
  
  if (!data_queue.Init("/bidirectional_data_queue", 50, true, true)) {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::cerr << "[数据提供方] 错误：无法初始化数据队列" << std::endl;
    g_running = false;
    return;
  }
  
  {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::cout << "[数据提供方] 初始化完成，等待命令..." << std::endl;
  }
  
  // 模拟内部状态
  double internal_param = 1.0;
  
  // 主循环
  CommandMessage cmd;
  while (g_running) {
    // 检查是否有新命令
    if (cmd_queue.Dequeue(&cmd)) {
      DataMessage response;
      response.response_to_cmd_id = cmd.cmd_id;
      
      {
        std::lock_guard<std::mutex> lock(g_console_mutex);
        std::cout << "[数据提供方] 收到命令 #" << cmd.cmd_id 
                  << " 类型: " << cmd.type 
                  << " 描述: " << cmd.description << std::endl;
      }
      
      // 处理不同类型的命令
      switch (cmd.type) {
        case CommandMessage::GET_DATA:
          response.success = true;
          // 生成一些模拟数据
          for (int i = 0; i < 10; i++) {
            response.value[i] = internal_param * (i + 1) * (rand() % 100) / 100.0;
          }
          snprintf(response.message, sizeof(response.message), 
                   "已生成数据，当前参数值: %.2f", internal_param);
          break;
          
        case CommandMessage::SET_PARAM:
          internal_param = cmd.param_value;
          response.success = true;
          snprintf(response.message, sizeof(response.message), 
                   "参数已更新为 %.2f", internal_param);
          break;
          
        case CommandMessage::SHUTDOWN:
          response.success = true;
          snprintf(response.message, sizeof(response.message), "正在关闭...");
          g_running = false;
          break;
          
        default:
          response.success = false;
          snprintf(response.message, sizeof(response.message), 
                   "未知命令类型: %d", cmd.type);
          break;
      }
      
      // 发送响应
      if (!data_queue.Enqueue(response)) {
        std::lock_guard<std::mutex> lock(g_console_mutex);
        std::cerr << "[数据提供方] 错误：无法发送响应，数据队列已满" << std::endl;
      } else {
        std::lock_guard<std::mutex> lock(g_console_mutex);
        std::cout << "[数据提供方] 已发送响应: " << response.message << std::endl;
      }
    }
    
    // 避免空转消耗CPU
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  
  {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::cout << "[数据提供方] 正在清理资源..." << std::endl;
  }
}

// 主函数充当命令发送方
int main() {
  // 设置随机数种子
  srand(static_cast<unsigned int>(time(nullptr)));
  
  // 处理中断信号
  signal(SIGINT, signalHandler);
  
  std::cout << "双向共享内存通信示例" << std::endl;
  std::cout << "按Ctrl+C终止程序" << std::endl;
  
  // 命令发送队列（命令发送方->数据提供方）
  omnirt::common::util::ShmBoundedSpscLockfreeQueue<CommandMessage> cmd_queue;
  
  // 数据接收队列（数据提供方->命令发送方）
  omnirt::common::util::ShmBoundedSpscLockfreeQueue<DataMessage> data_queue;
  
  // 初始化队列
  if (!cmd_queue.Init("/bidirectional_cmd_queue", 50, true, true)) {
    std::cerr << "[命令发送方] 错误：无法初始化命令队列" << std::endl;
    return 1;
  }
  
  if (!data_queue.Init("/bidirectional_data_queue", 50, true, false)) {
    std::cerr << "[命令发送方] 错误：无法连接到数据队列，请确保先运行数据提供方" << std::endl;
    return 1;
  }
  
  // 启动数据提供者线程（模拟另一个进程）
  std::thread provider_thread(dataProviderThread);
  
  // 给线程一些时间进行初始化
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  
  std::cout << "[命令发送方] 初始化完成，开始发送命令..." << std::endl;
  
  // 主循环：发送命令并接收响应
  int cmd_id = 1;
  DataMessage response;
  
  while (g_running) {
    // 创建一个命令
    CommandMessage cmd;
    cmd.cmd_id = cmd_id++;
    
    // 交替发送不同类型的命令
    switch (cmd_id % 3) {
      case 1:
        cmd.type = CommandMessage::GET_DATA;
        strncpy(cmd.description, "请发送当前数据", sizeof(cmd.description) - 1);
        break;
        
      case 2:
        cmd.type = CommandMessage::SET_PARAM;
        cmd.param_value = static_cast<double>(rand() % 1000) / 100.0;
        snprintf(cmd.description, sizeof(cmd.description) - 1, 
                 "设置参数为 %.2f", cmd.param_value);
        break;
        
      case 0:
        // 每第三个命令，询问是否退出
        if (cmd_id > 9) {
          cmd.type = CommandMessage::SHUTDOWN;
          strncpy(cmd.description, "请关闭系统", sizeof(cmd.description) - 1);
        } else {
          cmd.type = CommandMessage::GET_DATA;
          strncpy(cmd.description, "请发送更多数据", sizeof(cmd.description) - 1);
        }
        break;
    }
    
    // 发送命令
    {
      std::lock_guard<std::mutex> lock(g_console_mutex);
      std::cout << "[命令发送方] 发送命令 #" << cmd.cmd_id 
                << " 类型: " << cmd.type
                << " 描述: " << cmd.description << std::endl;
    }
    
    if (!cmd_queue.Enqueue(cmd)) {
      std::lock_guard<std::mutex> lock(g_console_mutex);
      std::cerr << "[命令发送方] 错误：无法发送命令，队列已满" << std::endl;
    }
    
    // 等待并接收响应
    bool got_response = false;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    while (g_running && !got_response) {
      if (data_queue.Dequeue(&response)) {
        if (response.response_to_cmd_id == cmd.cmd_id) {
          auto end_time = std::chrono::high_resolution_clock::now();
          double latency_ms = std::chrono::duration<double, std::milli>(
              end_time - start_time).count();
          
          {
            std::lock_guard<std::mutex> lock(g_console_mutex);
            std::cout << "[命令发送方] 收到响应 #" << response.response_to_cmd_id 
                      << " 延迟: " << latency_ms << "ms" 
                      << " 成功: " << (response.success ? "是" : "否") 
                      << " 消息: " << response.message << std::endl;
                      
            // 如果是数据命令，显示数据
            if (cmd.type == CommandMessage::GET_DATA && response.success) {
              std::cout << "[命令发送方] 收到数据: ";
              for (int i = 0; i < 5; ++i) {
                std::cout << response.value[i] << " ";
              }
              std::cout << "..." << std::endl;
            }
          }
          
          got_response = true;
        } else {
          std::lock_guard<std::mutex> lock(g_console_mutex);
          std::cout << "[命令发送方] 收到过期响应 #" << response.response_to_cmd_id 
                    << "，等待当前命令 #" << cmd.cmd_id << " 的响应" << std::endl;
        }
      } else {
        // 检查超时（500ms）
        auto current_time = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration<double, std::milli>(
                current_time - start_time).count() > 500) {
          std::lock_guard<std::mutex> lock(g_console_mutex);
          std::cout << "[命令发送方] 等待响应超时，继续下一个命令" << std::endl;
          break;
        }
        
        // 短暂休眠避免CPU空转
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
    
    // 在命令之间添加一些延迟
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
  
  std::cout << "[命令发送方] 程序结束，等待数据提供方线程退出..." << std::endl;
  
  // 等待线程结束
  if (provider_thread.joinable()) {
    provider_thread.join();
  }
  
  std::cout << "双向通信演示已完成" << std::endl;
  return 0;
}
