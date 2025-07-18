#pragma once

#include <curl/curl.h>
#include "../llm_base.h"

namespace aimrt {
namespace runtime {
namespace core {
namespace llm {

class OpenAILLM : public LLMBase {
 public:
  OpenAILLM();
  ~OpenAILLM() override;

  void Initialize(std::string_view name, YAML::Node options_node) override;
  void Start() override;
  void Shutdown() override;

  ChatResponse Chat(
      const std::vector<Message>& messages,
      const ChatOptions& options = ChatOptions()) noexcept override;
  
  ChatResponse StreamChat(
      const std::vector<Message>& messages,
      std::function<void(const std::string&)> callback,
      const ChatOptions& options = ChatOptions()) noexcept override;

  EmbeddingResponse Embedding(
      const std::string& text,
      const EmbeddingOptions& options = EmbeddingOptions()) noexcept override;
  
  std::vector<EmbeddingResponse> BatchEmbedding(
      const std::vector<std::string>& texts,
      const EmbeddingOptions& options = EmbeddingOptions()) noexcept override;

  void RegisterFunction(
      const std::string& name,
      const std::string& description,
      const nlohmann::json& parameters) override;

  void UnregisterFunction(const std::string& name) override;

  std::string GetModelName() const override { return model_name_; }
  int GetMaxContextLength() const override { return max_context_length_; }
  bool SupportsFunctionCalling() const override { return true; }
  bool SupportsStreaming() const override { return true; }

 private:
  static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
  nlohmann::json MakeRequest(const std::string& endpoint, const nlohmann::json& data);
  
 private:
  std::shared_ptr<aimrt::common::util::LoggerWrapper> logger_ptr_;
  std::string api_key_;
  std::string model_name_;
  std::string api_base_;
  int max_context_length_;
  CURL* curl_;
  std::unordered_map<std::string, nlohmann::json> registered_functions_;
};

}  // namespace llm
}  // namespace core
}  // namespace runtime
}  // namespace aimrt
