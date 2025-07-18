#include "aimrt/runtime/core/llm/llm_manager.h"
#include "aimrt/runtime/core/llm/llm_error.h"
#include <iostream>
#include <iomanip>
#include <chrono>

using namespace aimrt::runtime::core::llm;

// 比较不同LLM提供商的响应
void CompareLLMResponses(
    std::shared_ptr<LLMManager> llm_manager,
    const std::string& prompt) {
    
    std::cout << "\n=== Comparing LLM Responses ===" << std::endl;
    std::cout << "Prompt: " << prompt << std::endl;

    auto providers = llm_manager->GetAvailableLLMs();
    for (const auto& provider : providers) {
        std::cout << "\nProvider: " << provider << std::endl;
        std::cout << "----------------------------------------" << std::endl;

        auto llm = llm_manager->GetLLM(provider);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<Message> messages = {
            {Role::kUser, prompt}
        };
        
        auto response = llm->Chat(messages);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        std::cout << "Response: " << response.content << std::endl;
        std::cout << "Time taken: " << duration.count() << "ms" << std::endl;
        std::cout << "Tokens used: " << response.total_tokens << std::endl;
    }
}

// 测试不同模型的性能
void BenchmarkLLMs(
    std::shared_ptr<LLMManager> llm_manager,
    const std::vector<std::string>& test_cases) {
    
    std::cout << "\n=== LLM Performance Benchmark ===" << std::endl;

    struct BenchmarkResult {
        double avg_response_time;
        double avg_tokens_per_second;
        int total_tokens;
    };

    std::unordered_map<std::string, BenchmarkResult> results;
    
    auto providers = llm_manager->GetAvailableLLMs();
    for (const auto& provider : providers) {
        std::cout << "\nBenchmarking " << provider << "..." << std::endl;
        
        auto llm = llm_manager->GetLLM(provider);
        double total_time = 0;
        int total_tokens = 0;
        
        for (const auto& test : test_cases) {
            std::vector<Message> messages = {{Role::kUser, test}};
            
            auto start_time = std::chrono::high_resolution_clock::now();
            auto response = llm->Chat(messages);
            auto end_time = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time).count();
            
            total_time += duration;
            total_tokens += response.total_tokens;
        }

        double avg_time = total_time / test_cases.size();
        double tokens_per_second = (total_tokens / total_time) * 1000;  // Convert to seconds
        
        results[provider] = {
            avg_time,
            tokens_per_second,
            total_tokens
        };
    }

    // 打印结果
    std::cout << "\nBenchmark Results:" << std::endl;
    std::cout << std::setw(15) << "Provider" 
              << std::setw(20) << "Avg Response Time" 
              << std::setw(25) << "Tokens/Second"
              << std::setw(15) << "Total Tokens" << std::endl;
    std::cout << std::string(75, '-') << std::endl;

    for (const auto& [provider, result] : results) {
        std::cout << std::setw(15) << provider
                  << std::setw(20) << std::fixed << std::setprecision(2) 
                  << result.avg_response_time << "ms"
                  << std::setw(25) << result.avg_tokens_per_second
                  << std::setw(15) << result.total_tokens << std::endl;
    }
}

int main() {
    try {
        // 创建配置
        YAML::Node config;
        
        // OpenAI配置
        config["llm"]["openai"] = {
            {"api_key", std::getenv("OPENAI_API_KEY")},
            {"model", "gpt-3.5-turbo"},
            {"api_base", "https://api.openai.com/v1"},
            {"max_context_length", 4096}
        };

        // Gemini配置
        config["llm"]["gemini"] = {
            {"api_key", std::getenv("GEMINI_API_KEY")},
            {"model", "gemini-pro"},
            {"api_base", "https://generativelanguage.googleapis.com/v1"},
            {"max_context_length", 32768}
        };

        // 初始化LLM管理器
        auto llm_manager = std::make_shared<LLMManager>();
        llm_manager->Initialize(config);
        llm_manager->Start();

        // 比较不同LLM的响应
        CompareLLMResponses(llm_manager,
            "解释一下C++中的虚函数和多态。");

        // 性能测试用例
        std::vector<std::string> test_cases = {
            "什么是设计模式？",
            "解释一下RAII原则。",
            "C++11引入了哪些新特性？",
            "智能指针有哪些类型？",
            "什么是模板元编程？"
        };

        // 运行性能测试
        BenchmarkLLMs(llm_manager, test_cases);

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
