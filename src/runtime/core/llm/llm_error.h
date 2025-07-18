#pragma once

#include <system_error>
#include <string>

namespace aimrt {
namespace runtime {
namespace core {
namespace llm {

enum class LLMError {
  kSuccess = 0,
  kInvalidArgument = 1,
  kInvalidState = 2,
  kNetworkError = 3,
  kAPIError = 4,
  kInvalidResponse = 5,
  kTimeout = 6,
  kNotInitialized = 7,
  kNotImplemented = 8,
  kUnknownError = 9
};

// 错误类别实现
class LLMErrorCategory : public std::error_category {
 public:
  static const LLMErrorCategory& Instance() {
    static LLMErrorCategory instance;
    return instance;
  }

  const char* name() const noexcept override { return "LLMError"; }

  std::string message(int ev) const override {
    switch (static_cast<LLMError>(ev)) {
      case LLMError::kSuccess:
        return "Success";
      case LLMError::kInvalidArgument:
        return "Invalid argument";
      case LLMError::kInvalidState:
        return "Invalid state";
      case LLMError::kNetworkError:
        return "Network error";
      case LLMError::kAPIError:
        return "API error";
      case LLMError::kInvalidResponse:
        return "Invalid response";
      case LLMError::kTimeout:
        return "Operation timeout";
      case LLMError::kNotInitialized:
        return "Not initialized";
      case LLMError::kNotImplemented:
        return "Not implemented";
      case LLMError::kUnknownError:
        return "Unknown error";
      default:
        return "Unrecognized error";
    }
  }
};

inline std::error_code make_error_code(LLMError e) {
  return {static_cast<int>(e), LLMErrorCategory::Instance()};
}

// 自定义异常类
class LLMException : public std::runtime_error {
 public:
  explicit LLMException(LLMError error)
      : std::runtime_error(LLMErrorCategory::Instance().message(static_cast<int>(error))),
        error_(error) {}

  explicit LLMException(LLMError error, const std::string& what)
      : std::runtime_error(what),
        error_(error) {}

  LLMError error() const { return error_; }

 private:
  LLMError error_;
};

}  // namespace llm
}  // namespace core
}  // namespace runtime
}  // namespace aimrt

// 使std::error_code支持LLMError
namespace std {
template <>
struct is_error_code_enum<aimrt::runtime::core::llm::LLMError> : true_type {};
}  // namespace std
