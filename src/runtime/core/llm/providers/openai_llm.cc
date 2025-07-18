#include "openai_llm.h"
#include "../llm_error.h"
#include <sstream>
#include <mutex>

namespace aimrt {
namespace runtime {
namespace core {
namespace llm {

namespace {
constexpr char kLoggerName[] = "OpenAILLM";
constexpr char kDefaultAPIBase[] = "https://api.openai.com/v1";
constexpr int kDefaultMaxContextLength = 4096;
constexpr int kDefaultTimeout = 30;  // 30 seconds
}  // namespace

OpenAILLM::OpenAILLM()
    : logger_ptr_(std::make_shared<aimrt::common::util::LoggerWrapper>()),
      max_context_length_(kDefaultMaxContextLength),
      curl_(nullptr) {
}

OpenAILLM::~OpenAILLM() {
  if (curl_) {
    curl_easy_cleanup(curl_);
    curl_ = nullptr;
  }
}

void OpenAILLM::Initialize(std::string_view name, YAML::Node options_node) {
  std::lock_guard<std::mutex> lock(mutex_);

  try {
    // 初始化日志
    logger_ptr_->Initialize(fmt::format("{}.{}", kLoggerName, name));

    // 解析配置
    api_key_ = options_node["api_key"].as<std::string>();
    model_name_ = options_node["model"].as<std::string>();
    api_base_ = options_node["api_base"].as<std::string>(kDefaultAPIBase);
    max_context_length_ = options_node["max_context_length"].as<int>(kDefaultMaxContextLength);

    // 初始化CURL
    curl_ = curl_easy_init();
    if (!curl_) {
      throw LLMException(LLMError::kUnknownError, "Failed to initialize CURL");
    }

    // 设置CURL选项
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, kDefaultTimeout);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
    
    // 设置HTTP头
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, 
        fmt::format("Authorization: Bearer {}", api_key_).c_str());
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);

    logger_ptr_->Info("OpenAILLM initialized successfully");
  } catch (const std::exception& e) {
    logger_ptr_->Error("Failed to initialize OpenAILLM: {}", e.what());
    throw;
  }
}

void OpenAILLM::Start() {
  std::lock_guard<std::mutex> lock(mutex_);
  logger_ptr_->Info("OpenAILLM started");
}

void OpenAILLM::Shutdown() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (curl_) {
    curl_easy_cleanup(curl_);
    curl_ = nullptr;
  }
  
  logger_ptr_->Info("OpenAILLM shutdown");
}

ChatResponse OpenAILLM::Chat(
    const std::vector<Message>& messages,
    const ChatOptions& options) noexcept {
  try {
    nlohmann::json request = {
      {"model", options.model.empty() ? model_name_ : options.model},
      {"temperature", options.temperature},
      {"max_tokens", options.max_tokens},
      {"top_p", options.top_p},
      {"presence_penalty", options.presence_penalty},
      {"frequency_penalty", options.frequency_penalty},
      {"messages", nlohmann::json::array()}
    };

    // 转换消息格式
    for (const auto& msg : messages) {
      nlohmann::json message = {
        {"role", RoleToString(msg.role)},
        {"content", msg.content}
      };
      
      if (!msg.name.empty()) {
        message["name"] = msg.name;
      }
      
      if (!msg.function_call.empty()) {
        message["function_call"] = msg.function_call;
      }
      
      request["messages"].push_back(message);
    }

    // 添加函数定义
    if (!registered_functions_.empty()) {
      request["functions"] = nlohmann::json::array();
      for (const auto& [name, func] : registered_functions_) {
        request["functions"].push_back(func);
      }
    }

    // 发送请求
    auto response = MakeRequest("/chat/completions", request);

    // 解析响应
    ChatResponse result;
    auto choices = response["choices"];
    if (!choices.empty()) {
      auto& choice = choices[0];
      result.content = choice["message"]["content"].get<std::string>();
      result.finish_reason = choice["finish_reason"].get<std::string>();
      
      if (choice["message"].contains("function_call")) {
        result.function_call = choice["message"]["function_call"];
      }
    }

    auto usage = response["usage"];
    result.prompt_tokens = usage["prompt_tokens"].get<int>();
    result.completion_tokens = usage["completion_tokens"].get<int>();
    result.total_tokens = usage["total_tokens"].get<int>();

    return result;
  } catch (const std::exception& e) {
    logger_ptr_->Error("Chat failed: {}", e.what());
    return ChatResponse{};
  }
}

ChatResponse OpenAILLM::StreamChat(
    const std::vector<Message>& messages,
    std::function<void(const std::string&)> callback,
    const ChatOptions& options) noexcept {
  try {
    nlohmann::json request = {
      {"model", options.model.empty() ? model_name_ : options.model},
      {"temperature", options.temperature},
      {"max_tokens", options.max_tokens},
      {"top_p", options.top_p},
      {"presence_penalty", options.presence_penalty},
      {"frequency_penalty", options.frequency_penalty},
      {"stream", true},
      {"messages", nlohmann::json::array()}
    };

    // 转换消息格式
    for (const auto& msg : messages) {
      request["messages"].push_back({
        {"role", RoleToString(msg.role)},
        {"content", msg.content}
      });
    }

    // 设置回调
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, 
        [](void* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
          auto* callback = static_cast<std::function<void(const std::string&)>*>(userdata);
          std::string data(static_cast<char*>(ptr), size * nmemb);
          (*callback)(data);
          return size * nmemb;
        });
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &callback);

    // 发送请求
    auto response = MakeRequest("/chat/completions", request);

    // 返回最终结果
    ChatResponse result;
    // ... 解析最终结果 ...
    return result;
  } catch (const std::exception& e) {
    logger_ptr_->Error("StreamChat failed: {}", e.what());
    return ChatResponse{};
  }
}

EmbeddingResponse OpenAILLM::Embedding(
    const std::string& text,
    const EmbeddingOptions& options) noexcept {
  try {
    nlohmann::json request = {
      {"model", options.model.empty() ? "text-embedding-ada-002" : options.model},
      {"input", text},
      {"encoding_format", options.encoding_format}
    };

    auto response = MakeRequest("/embeddings", request);

    EmbeddingResponse result;
    if (!response["data"].empty()) {
      result.embedding = response["data"][0]["embedding"].get<std::vector<float>>();
      result.tokens = response["usage"]["total_tokens"].get<int>();
    }

    return result;
  } catch (const std::exception& e) {
    logger_ptr_->Error("Embedding failed: {}", e.what());
    return EmbeddingResponse{};
  }
}

std::vector<EmbeddingResponse> OpenAILLM::BatchEmbedding(
    const std::vector<std::string>& texts,
    const EmbeddingOptions& options) noexcept {
  try {
    nlohmann::json request = {
      {"model", options.model.empty() ? "text-embedding-ada-002" : options.model},
      {"input", texts},
      {"encoding_format", options.encoding_format}
    };

    auto response = MakeRequest("/embeddings", request);

    std::vector<EmbeddingResponse> results;
    results.reserve(texts.size());

    for (const auto& data : response["data"]) {
      EmbeddingResponse result;
      result.embedding = data["embedding"].get<std::vector<float>>();
      result.tokens = response["usage"]["total_tokens"].get<int>() / texts.size();
      results.push_back(std::move(result));
    }

    return results;
  } catch (const std::exception& e) {
    logger_ptr_->Error("BatchEmbedding failed: {}", e.what());
    return std::vector<EmbeddingResponse>{};
  }
}

void OpenAILLM::RegisterFunction(
    const std::string& name,
    const std::string& description,
    const nlohmann::json& parameters) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  nlohmann::json function = {
    {"name", name},
    {"description", description},
    {"parameters", parameters}
  };
  
  registered_functions_[name] = std::move(function);
  logger_ptr_->Info("Registered function '{}'", name);
}

void OpenAILLM::UnregisterFunction(const std::string& name) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = registered_functions_.find(name);
  if (it != registered_functions_.end()) {
    registered_functions_.erase(it);
    logger_ptr_->Info("Unregistered function '{}'", name);
  }
}

size_t OpenAILLM::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
  auto* response = static_cast<std::string*>(userp);
  size_t total_size = size * nmemb;
  response->append(static_cast<char*>(contents), total_size);
  return total_size;
}

nlohmann::json OpenAILLM::MakeRequest(
    const std::string& endpoint,
    const nlohmann::json& data) {
  std::lock_guard<std::mutex> lock(mutex_);

  std::string url = api_base_ + endpoint;
  std::string request_data = data.dump();
  std::string response_data;

  curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, request_data.c_str());
  curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_data);

  CURLcode res = curl_easy_perform(curl_);
  if (res != CURLE_OK) {
    throw LLMException(LLMError::kNetworkError,
        fmt::format("CURL request failed: {}", curl_easy_strerror(res)));
  }

  long response_code;
  curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code);
  if (response_code != 200) {
    throw LLMException(LLMError::kAPIError,
        fmt::format("API request failed with code {}: {}", 
            response_code, response_data));
  }

  try {
    return nlohmann::json::parse(response_data);
  } catch (const nlohmann::json::exception& e) {
    throw LLMException(LLMError::kInvalidResponse,
        fmt::format("Failed to parse response: {}", e.what()));
  }
}

std::string OpenAILLM::RoleToString(Role role) {
  switch (role) {
    case Role::kSystem:
      return "system";
    case Role::kUser:
      return "user";
    case Role::kAssistant:
      return "assistant";
    case Role::kFunction:
      return "function";
    default:
      return "user";
  }
}

}  // namespace llm
}  // namespace core
}  // namespace runtime
}  // namespace aimrt
