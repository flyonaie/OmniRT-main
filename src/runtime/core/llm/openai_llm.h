#pragma once

#include "llm_base.h"

namespace aimrt {
namespace runtime {
namespace core {
namespace llm {

class OpenAILLM : public LLMBase {
 public:
  OpenAILLM()
      : logger_ptr_(std::make_shared<aimrt::common::util::LoggerWrapper>()) {}
  ~OpenAILLM() override = default;

  void Initialize(std::string_view name, YAML::Node options_node) override;
  void Start() override;
  void Shutdown() override;

  std::string Chat(const std::string& prompt) noexcept override;
  std::string Embedding(const std::string& text) noexcept override;

 private:
  std::shared_ptr<aimrt::common::util::LoggerWrapper> logger_ptr_;
  std::string api_key_;
  std::string model_name_;
  std::string api_endpoint_;
};

}  // namespace llm
}  // namespace core
}  // namespace runtime
}  // namespace aimrt
