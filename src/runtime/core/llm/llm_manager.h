#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <yaml-cpp/yaml.h>
#include "aimrt/common/util/logger_wrapper.h"
#include "aimrt/common/util/string_util.h"
#include "llm_types.h"
#include "llm_base.h"
#include "providers/openai_llm.h"
#include "providers/gemini_llm.h"

namespace aimrt {
namespace runtime {
namespace core {
namespace llm {

class LLMManager {
 public:
  using LLMGenFunc = std::function<std::unique_ptr<LLMBase>()>;

  enum class State : uint32_t {
    kPreInit,
    kInit,
    kStart,
    kShutdown,
  };

 public:
  LLMManager()
      : logger_ptr_(std::make_shared<aimrt::common::util::LoggerWrapper>()),
        state_(State::kPreInit) {}
  ~LLMManager() = default;

  LLMManager(const LLMManager&) = delete;
  LLMManager& operator=(const LLMManager&) = delete;

  void Initialize(YAML::Node options_node);
  void Start();
  void Shutdown();

  // 注册LLM生成函数
  void RegisterLLMGenFunc(std::string_view type, LLMGenFunc&& llm_gen_func);

  // 获取LLM实例
  std::shared_ptr<LLMBase> GetLLM(std::string_view llm_name);

  // 获取所有可用的LLM名称
  std::vector<std::string> GetAvailableLLMs() const;

  // 获取LLM配置
  YAML::Node GetLLMConfig(std::string_view llm_name) const;

 private:
  void RegisterBuiltinLLMs();
  void RegisterOpenAILLMGenFunc();
  void RegisterGeminiLLMGenFunc();
  void ValidateConfig(const YAML::Node& config) const;

 private:
  std::shared_ptr<aimrt::common::util::LoggerWrapper> logger_ptr_;
  std::atomic<State> state_;
  YAML::Node options_node_;

  // LLM生成函数映射
  std::unordered_map<std::string, LLMGenFunc> llm_gen_func_map_;

  // LLM实例映射
  std::unordered_map<
      std::string,
      std::shared_ptr<LLMBase>,
      aimrt::common::util::StringHash,
      aimrt::common::util::StringEqual>
      llm_instance_map_;

  // LLM配置映射
  std::unordered_map<
      std::string,
      YAML::Node,
      aimrt::common::util::StringHash,
      aimrt::common::util::StringEqual>
      llm_config_map_;
};

}  // namespace llm
}  // namespace core
}  // namespace runtime
}  // namespace aimrt
