#pragma once

#include "aimrt_module_c_interface/llm_base.h"
#include "../util/common.h"

namespace aimrt {
namespace llm {

class LlmHandleRef {
 public:
  LlmHandleRef() = default;
  explicit LlmHandleRef(const aimrt_llm_handle_t* handle) : handle_(handle) {}
  ~LlmHandleRef() = default;

  explicit operator bool() const { return (handle_ != nullptr); }

  // 获取原生句柄
  const aimrt_llm_handle_t* NativeHandle() const { return handle_; }

  // 文本生成接口
  std::string Generate(const std::string& prompt, 
                      const std::map<std::string, std::string>& params = {}) const {
    AIMRT_ASSERT(handle_, "Reference is null.");
    // 实现文本生成的调用
    return std::string();  // TODO: 实现实际调用
  }

  // 向量嵌入接口
  std::vector<float> Embedding(const std::string& text) const {
    AIMRT_ASSERT(handle_, "Reference is null.");
    // 实现文本嵌入的调用
    return std::vector<float>();  // TODO: 实现实际调用
  }

private:
  const aimrt_llm_handle_t* handle_ = nullptr;
};

}  // namespace llm
}  // namespace aimrt
