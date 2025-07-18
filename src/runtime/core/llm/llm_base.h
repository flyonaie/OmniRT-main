#pragma once

#include <string>
#include <memory>
#include <vector>
#include <yaml-cpp/yaml.h>
#include "aimrt/common/util/logger_wrapper.h"
#include "llm_types.h"

namespace aimrt {
namespace runtime {
namespace core {
namespace llm {

class LLMBase {
 public:
  LLMBase() = default;
  virtual ~LLMBase() = default;

  LLMBase(const LLMBase&) = delete;
  LLMBase& operator=(const LLMBase&) = delete;

  virtual void Initialize(std::string_view name, YAML::Node options_node) = 0;
  virtual void Start() = 0;
  virtual void Shutdown() = 0;

  // 聊天相关接口
  virtual ChatResponse Chat(
      const std::vector<Message>& messages,
      const ChatOptions& options = ChatOptions()) noexcept = 0;
  
  virtual ChatResponse StreamChat(
      const std::vector<Message>& messages,
      std::function<void(const std::string&)> callback,
      const ChatOptions& options = ChatOptions()) noexcept = 0;

  // Embedding相关接口
  virtual EmbeddingResponse Embedding(
      const std::string& text,
      const EmbeddingOptions& options = EmbeddingOptions()) noexcept = 0;
  
  virtual std::vector<EmbeddingResponse> BatchEmbedding(
      const std::vector<std::string>& texts,
      const EmbeddingOptions& options = EmbeddingOptions()) noexcept = 0;

  // 函数调用相关接口
  virtual void RegisterFunction(
      const std::string& name,
      const std::string& description,
      const nlohmann::json& parameters) = 0;

  virtual void UnregisterFunction(const std::string& name) = 0;

  // 模型信息接口
  virtual std::string GetModelName() const = 0;
  virtual int GetMaxContextLength() const = 0;
  virtual bool SupportsFunctionCalling() const = 0;
  virtual bool SupportsStreaming() const = 0;
};

}  // namespace llm
}  // namespace core
}  // namespace runtime
}  // namespace aimrt
