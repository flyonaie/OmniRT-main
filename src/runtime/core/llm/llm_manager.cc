#include "llm_manager.h"
#include "llm_error.h"
#include <mutex>

namespace aimrt {
namespace runtime {
namespace core {
namespace llm {

namespace {
constexpr char kLoggerName[] = "LLMManager";
}  // namespace

void LLMManager::Initialize(YAML::Node options_node) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (state_ != State::kPreInit) {
    throw LLMException(LLMError::kInvalidState, "LLMManager already initialized");
  }

  try {
    ValidateConfig(options_node);
    options_node_ = options_node;
    
    // 初始化日志
    logger_ptr_->Initialize(kLoggerName);
    
    // 注册内置LLM
    RegisterBuiltinLLMs();
    
    state_ = State::kInit;
    logger_ptr_->Info("LLMManager initialized successfully");
  } catch (const std::exception& e) {
    logger_ptr_->Error("Failed to initialize LLMManager: {}", e.what());
    throw;
  }
}

void LLMManager::Start() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (state_ != State::kInit) {
    throw LLMException(LLMError::kInvalidState, "LLMManager not initialized");
  }

  try {
    // 初始化所有配置的LLM实例
    auto llm_configs = options_node_["llm"];
    if (llm_configs && llm_configs.IsMap()) {
      for (const auto& llm_config : llm_configs) {
        const auto& name = llm_config.first.as<std::string>();
        const auto& config = llm_config.second;
        
        // 存储配置
        llm_config_map_[name] = config;
        
        // 如果有对应的生成函数，创建实例
        auto it = llm_gen_func_map_.find(name);
        if (it != llm_gen_func_map_.end()) {
          auto llm = it->second();
          llm->Initialize(name, config);
          llm->Start();
          llm_instance_map_[name] = std::move(llm);
          logger_ptr_->Info("LLM instance '{}' started", name);
        }
      }
    }
    
    state_ = State::kStart;
    logger_ptr_->Info("LLMManager started successfully");
  } catch (const std::exception& e) {
    logger_ptr_->Error("Failed to start LLMManager: {}", e.what());
    throw;
  }
}

void LLMManager::Shutdown() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (state_ != State::kStart) {
    return;
  }

  try {
    // 关闭所有LLM实例
    for (auto& [name, llm] : llm_instance_map_) {
      try {
        llm->Shutdown();
        logger_ptr_->Info("LLM instance '{}' shutdown", name);
      } catch (const std::exception& e) {
        logger_ptr_->Error("Failed to shutdown LLM instance '{}': {}", name, e.what());
      }
    }
    
    llm_instance_map_.clear();
    llm_config_map_.clear();
    
    state_ = State::kShutdown;
    logger_ptr_->Info("LLMManager shutdown successfully");
  } catch (const std::exception& e) {
    logger_ptr_->Error("Failed to shutdown LLMManager: {}", e.what());
    throw;
  }
}

void LLMManager::RegisterLLMGenFunc(std::string_view type, LLMGenFunc&& llm_gen_func) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (!llm_gen_func) {
    throw LLMException(LLMError::kInvalidArgument, "Invalid LLM generator function");
  }

  std::string type_str(type);
  llm_gen_func_map_[type_str] = std::move(llm_gen_func);
  logger_ptr_->Info("Registered LLM generator function for type '{}'", type_str);
}

std::shared_ptr<LLMBase> LLMManager::GetLLM(std::string_view llm_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (state_ != State::kStart) {
    throw LLMException(LLMError::kInvalidState, "LLMManager not started");
  }

  std::string name_str(llm_name);
  auto it = llm_instance_map_.find(name_str);
  if (it == llm_instance_map_.end()) {
    throw LLMException(LLMError::kInvalidArgument, 
        fmt::format("LLM '{}' not found", name_str));
  }

  return it->second;
}

std::vector<std::string> LLMManager::GetAvailableLLMs() const {
  std::lock_guard<std::mutex> lock(mutex_);
  
  std::vector<std::string> llms;
  llms.reserve(llm_instance_map_.size());
  
  for (const auto& [name, _] : llm_instance_map_) {
    llms.push_back(name);
  }
  
  return llms;
}

YAML::Node LLMManager::GetLLMConfig(std::string_view llm_name) const {
  std::lock_guard<std::mutex> lock(mutex_);
  
  std::string name_str(llm_name);
  auto it = llm_config_map_.find(name_str);
  if (it == llm_config_map_.end()) {
    throw LLMException(LLMError::kInvalidArgument, 
        fmt::format("Configuration for LLM '{}' not found", name_str));
  }
  
  return it->second;
}

void LLMManager::RegisterBuiltinLLMs() {
  RegisterOpenAILLMGenFunc();
  RegisterGeminiLLMGenFunc();
}

void LLMManager::RegisterOpenAILLMGenFunc() {
  RegisterLLMGenFunc("openai", []() {
    return std::make_unique<OpenAILLM>();
  });
}

void LLMManager::RegisterGeminiLLMGenFunc() {
  RegisterLLMGenFunc("gemini", []() {
    return std::make_unique<GeminiLLM>();
  });
}

void LLMManager::ValidateConfig(const YAML::Node& config) const {
  if (!config["llm"] || !config["llm"].IsMap()) {
    throw LLMException(LLMError::kInvalidArgument, "Missing or invalid 'llm' section in config");
  }

  for (const auto& llm_config : config["llm"]) {
    const auto& name = llm_config.first.as<std::string>();
    const auto& config = llm_config.second;
    
    if (!config["api_key"] || !config["api_key"].IsScalar()) {
      throw LLMException(LLMError::kInvalidArgument, 
          fmt::format("Missing or invalid 'api_key' for LLM '{}'", name));
    }
    
    if (!config["model"] || !config["model"].IsScalar()) {
      throw LLMException(LLMError::kInvalidArgument, 
          fmt::format("Missing or invalid 'model' for LLM '{}'", name));
    }
  }
}

}  // namespace llm
}  // namespace core
}  // namespace runtime
}  // namespace aimrt
