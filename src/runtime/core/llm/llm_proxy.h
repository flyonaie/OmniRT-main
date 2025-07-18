#pragma once

#include <memory>
#include <string>
#include "aimrt/common/util/string_util.h"

namespace aimrt {
namespace runtime {
namespace core {
namespace llm {

// C接口定义
typedef struct {
  void (*chat)(void* impl, const char* prompt, char* response, size_t max_length);
  void (*embedding)(void* impl, const char* text, char* vector, size_t max_length);
  void* impl;
} aimrt_llm_base_t;

typedef struct {
  const aimrt_llm_base_t* (*get_llm)(void* impl, aimrt_string_view_t llm_name);
  void* impl;
} aimrt_llm_manager_base_t;

// C++接口封装
class LLMRef {
 public:
  LLMRef() = default;
  explicit LLMRef(const aimrt_llm_base_t* base_ptr)
      : base_ptr_(base_ptr) {}
  ~LLMRef() = default;

  explicit operator bool() const { return (base_ptr_ != nullptr); }
  const aimrt_llm_base_t* NativeHandle() const { return base_ptr_; }

  std::string Chat(const std::string& prompt) {
    char response[4096] = {0};  // 假设最大响应长度为4096
    base_ptr_->chat(base_ptr_->impl, prompt.c_str(), response, sizeof(response));
    return std::string(response);
  }

  std::string Embedding(const std::string& text) {
    char vector[1024] = {0};  // 假设向量长度为1024
    base_ptr_->embedding(base_ptr_->impl, text.c_str(), vector, sizeof(vector));
    return std::string(vector);
  }

 private:
  const aimrt_llm_base_t* base_ptr_ = nullptr;
};

// Proxy实现
class LLMProxy {
 public:
  explicit LLMProxy()
      : base_(GenBase(this)) {}
  ~LLMProxy() = default;

  LLMProxy(const LLMProxy&) = delete;
  LLMProxy& operator=(const LLMProxy&) = delete;

  const aimrt_llm_base_t* NativeHandle() const { return &base_; }

 private:
  static void Chat(void* impl, const char* prompt, char* response, size_t max_length) {
    // 实现chat功能
    std::string result = "LLM response to: " + std::string(prompt);
    strncpy(response, result.c_str(), max_length - 1);
  }

  static void Embedding(void* impl, const char* text, char* vector, size_t max_length) {
    // 实现embedding功能
    std::string result = "Embedding for: " + std::string(text);
    strncpy(vector, result.c_str(), max_length - 1);
  }

  static aimrt_llm_base_t GenBase(void* impl) {
    return aimrt_llm_base_t{
        .chat = [](void* impl, const char* prompt, char* response, size_t max_length) {
          return static_cast<LLMProxy*>(impl)->Chat(impl, prompt, response, max_length);
        },
        .embedding = [](void* impl, const char* text, char* vector, size_t max_length) {
          return static_cast<LLMProxy*>(impl)->Embedding(impl, text, vector, max_length);
        },
        .impl = impl};
  }

 private:
  const aimrt_llm_base_t base_;
};

}  // namespace llm
}  // namespace core
}  // namespace runtime
}  // namespace aimrt
