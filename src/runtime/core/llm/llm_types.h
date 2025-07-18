#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace aimrt {
namespace runtime {
namespace core {
namespace llm {

// 消息角色
enum class Role {
  kSystem,    // 系统消息
  kUser,      // 用户消息
  kAssistant, // 助手消息
  kFunction   // 函数调用
};

// 消息结构
struct Message {
  Role role;
  std::string content;
  std::string name;        // 用于function calling
  nlohmann::json function_call;
};

// 聊天选项
struct ChatOptions {
  std::string model;           // 模型名称
  float temperature = 0.7f;    // 温度
  int max_tokens = 2048;      // 最大token数
  float top_p = 1.0f;         // 采样概率
  float presence_penalty = 0;  // 存在惩罚
  float frequency_penalty = 0; // 频率惩罚
  std::vector<std::string> stop; // 停止词
};

// Embedding选项
struct EmbeddingOptions {
  std::string model;          // 模型名称
  std::string encoding_format = "float"; // 编码格式
};

// 聊天响应
struct ChatResponse {
  std::string content;        // 响应内容
  std::string finish_reason;  // 结束原因
  int prompt_tokens;          // 提示token数
  int completion_tokens;      // 完成token数
  int total_tokens;          // 总token数
  nlohmann::json function_call; // 函数调用结果
};

// Embedding响应
struct EmbeddingResponse {
  std::vector<float> embedding;  // 向量
  int tokens;                    // token数
};

}  // namespace llm
}  // namespace core
}  // namespace runtime
}  // namespace aimrt
