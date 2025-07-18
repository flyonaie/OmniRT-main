#include "aimrt/runtime/core/llm/llm_manager.h"
#include "aimrt/runtime/core/llm/llm_error.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using namespace aimrt::runtime::core::llm;

// 简单的聊天示例
void SimpleChatExample(std::shared_ptr<LLMBase> llm) {
    std::cout << "\n=== Simple Chat Example ===" << std::endl;
    
    std::vector<Message> messages = {
        {Role::kSystem, "你是一个专业的C++编程助手。"},
        {Role::kUser, "请解释一下智能指针的作用和类型。"}
    };

    ChatOptions options;
    options.temperature = 0.7;
    
    auto response = llm->Chat(messages, options);
    std::cout << "Assistant: " << response.content << std::endl;
    std::cout << "Tokens used: " << response.total_tokens << std::endl;
}

// 流式聊天示例
void StreamChatExample(std::shared_ptr<LLMBase> llm) {
    std::cout << "\n=== Stream Chat Example ===" << std::endl;
    
    std::vector<Message> messages = {
        {Role::kSystem, "你是一个专业的C++编程助手。"},
        {Role::kUser, "请用通俗的语言解释一下设计模式中的工厂模式。"}
    };

    ChatOptions options;
    options.temperature = 0.7;
    
    std::cout << "Assistant: ";
    llm->StreamChat(messages, 
        [](const std::string& chunk) {
            std::cout << chunk << std::flush;
        },
        options
    );
    std::cout << std::endl;
}

// 函数调用示例
void FunctionCallExample(std::shared_ptr<LLMBase> llm) {
    std::cout << "\n=== Function Call Example ===" << std::endl;
    
    // 注册获取时间的函数
    llm->RegisterFunction(
        "get_current_time",
        "Get the current time in the specified timezone",
        {
            {"type", "object"},
            {"properties", {
                {"timezone", {
                    {"type", "string"},
                    {"enum", {"UTC", "local"}}
                }}
            }},
            {"required", {"timezone"}}
        }
    );

    std::vector<Message> messages = {
        {Role::kSystem, "你是一个助手，可以获取当前时间。"},
        {Role::kUser, "现在是什么时间？"}
    };

    ChatOptions options;
    auto response = llm->Chat(messages, options);
    
    if (!response.function_call.empty()) {
        std::cout << "Function called: " << response.function_call["name"] << std::endl;
        std::cout << "Arguments: " << response.function_call["arguments"] << std::endl;
    }
}

// Embedding示例
void EmbeddingExample(std::shared_ptr<LLMBase> llm) {
    std::cout << "\n=== Embedding Example ===" << std::endl;
    
    std::string text = "C++是一种强大的编程语言。";
    auto response = llm->Embedding(text);
    
    std::cout << "Text: " << text << std::endl;
    std::cout << "Embedding size: " << response.embedding.size() << std::endl;
    std::cout << "First 5 dimensions: ";
    for (size_t i = 0; i < 5 && i < response.embedding.size(); ++i) {
        std::cout << response.embedding[i] << " ";
    }
    std::cout << std::endl;
}

// 批量Embedding示例
void BatchEmbeddingExample(std::shared_ptr<LLMBase> llm) {
    std::cout << "\n=== Batch Embedding Example ===" << std::endl;
    
    std::vector<std::string> texts = {
        "C++是一种编程语言",
        "Python也是一种编程语言",
        "Java同样是一种编程语言"
    };
    
    auto responses = llm->BatchEmbedding(texts);
    
    for (size_t i = 0; i < responses.size(); ++i) {
        std::cout << "Text " << i + 1 << " embedding size: " 
                 << responses[i].embedding.size() << std::endl;
    }
}

int main() {
    try {
        // 创建配置
        YAML::Node config;
        config["llm"]["openai"] = {
            {"api_key", std::getenv("OPENAI_API_KEY")},  // 从环境变量获取API密钥
            {"model", "gpt-3.5-turbo"},
            {"api_base", "https://api.openai.com/v1"},
            {"max_context_length", 4096}
        };

        // 初始化LLM管理器
        auto llm_manager = std::make_shared<LLMManager>();
        llm_manager->Initialize(config);
        llm_manager->Start();

        // 获取OpenAI LLM实例
        auto llm = llm_manager->GetLLM("openai");

        // 运行示例
        SimpleChatExample(llm);
        StreamChatExample(llm);
        FunctionCallExample(llm);
        EmbeddingExample(llm);
        BatchEmbeddingExample(llm);

        // 关闭LLM管理器
        llm_manager->Shutdown();
        
        return 0;
    } catch (const LLMException& e) {
        std::cerr << "LLM error: " << e.what() 
                 << " (code: " << static_cast<int>(e.error()) << ")" 
                 << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
