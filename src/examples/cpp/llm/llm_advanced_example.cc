#include "aimrt/runtime/core/llm/llm_manager.h"
#include "aimrt/runtime/core/llm/llm_error.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

using namespace aimrt::runtime::core::llm;

// 线程安全的消息队列
template<typename T>
class ThreadSafeQueue {
public:
    void push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(item));
        cv_.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty(); });
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

// 聊天会话管理器
class ChatSessionManager {
public:
    explicit ChatSessionManager(std::shared_ptr<LLMBase> llm)
        : llm_(llm), running_(true) {
        // 启动处理线程
        worker_thread_ = std::thread(&ChatSessionManager::ProcessMessages, this);
    }

    ~ChatSessionManager() {
        running_ = false;
        message_queue_.push({}); // 发送空消息以唤醒处理线程
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    // 添加消息到会话
    void AddMessage(const std::string& session_id, const Message& message) {
        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            sessions_[session_id] = std::vector<Message>();
        }
        sessions_[session_id].push_back(message);
        
        // 将消息加入处理队列
        message_queue_.push({session_id, message});
    }

    // 获取会话历史
    std::vector<Message> GetSessionHistory(const std::string& session_id) const {
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            return it->second;
        }
        return {};
    }

    // 清除会话历史
    void ClearSession(const std::string& session_id) {
        sessions_.erase(session_id);
    }

private:
    void ProcessMessages() {
        while (running_) {
            auto msg_pair = message_queue_.pop();
            if (!running_) break;

            const auto& [session_id, message] = msg_pair;
            if (message.role == Role::kUser) {
                try {
                    auto history = GetSessionHistory(session_id);
                    
                    ChatOptions options;
                    options.temperature = 0.7;
                    
                    auto response = llm_->Chat(history, options);
                    
                    // 添加助手的回复到会话历史
                    AddMessage(session_id, {
                        Role::kAssistant,
                        response.content
                    });
                    
                    // 处理函数调用
                    if (!response.function_call.empty()) {
                        HandleFunctionCall(session_id, response.function_call);
                    }
                    
                } catch (const std::exception& e) {
                    std::cerr << "Error processing message in session " 
                              << session_id << ": " << e.what() << std::endl;
                }
            }
        }
    }

    void HandleFunctionCall(
        const std::string& session_id,
        const nlohmann::json& function_call) {
        
        const auto& func_name = function_call["name"].get<std::string>();
        const auto& args = function_call["arguments"].get<std::string>();

        // 这里可以实现具体的函数调用逻辑
        std::string result = "Function " + func_name + " called with args: " + args;
        
        // 将函数调用结果添加到会话历史
        AddMessage(session_id, {
            Role::kFunction,
            result,
            func_name
        });
    }

private:
    std::shared_ptr<LLMBase> llm_;
    std::atomic<bool> running_;
    std::thread worker_thread_;
    
    ThreadSafeQueue<std::pair<std::string, Message>> message_queue_;
    
    std::unordered_map<std::string, std::vector<Message>> sessions_;
    mutable std::mutex sessions_mutex_;
};

// 代码分析助手
class CodeAnalysisAssistant {
public:
    explicit CodeAnalysisAssistant(std::shared_ptr<LLMBase> llm)
        : llm_(llm) {}

    // 分析代码并提供建议
    std::string AnalyzeCode(const std::string& code) {
        std::vector<Message> messages = {
            {Role::kSystem, "你是一个专业的C++代码分析助手，请分析下面的代码并提供改进建议。"},
            {Role::kUser, "请分析这段代码：\n\n" + code}
        };

        ChatOptions options;
        options.temperature = 0.3;  // 使用较低的temperature以获得更确定性的回答
        
        auto response = llm_->Chat(messages, options);
        return response.content;
    }

    // 生成单元测试
    std::string GenerateUnitTests(const std::string& code) {
        std::vector<Message> messages = {
            {Role::kSystem, "你是一个专业的C++测试专家，请为下面的代码生成单元测试。"},
            {Role::kUser, "请为这段代码生成单元测试：\n\n" + code}
        };

        ChatOptions options;
        options.temperature = 0.3;
        
        auto response = llm_->Chat(messages, options);
        return response.content;
    }

    // 代码文档生成
    std::string GenerateDocumentation(const std::string& code) {
        std::vector<Message> messages = {
            {Role::kSystem, "你是一个专业的技术文档撰写专家，请为下面的代码生成详细的文档。"},
            {Role::kUser, "请为这段代码生成文档：\n\n" + code}
        };

        ChatOptions options;
        options.temperature = 0.3;
        
        auto response = llm_->Chat(messages, options);
        return response.content;
    }

private:
    std::shared_ptr<LLMBase> llm_;
};

int main() {
    try {
        // 创建配置
        YAML::Node config;
        config["llm"]["openai"] = {
            {"api_key", std::getenv("OPENAI_API_KEY")},
            {"model", "gpt-3.5-turbo"},
            {"api_base", "https://api.openai.com/v1"}
        };

        // 初始化LLM管理器
        auto llm_manager = std::make_shared<LLMManager>();
        llm_manager->Initialize(config);
        llm_manager->Start();

        auto llm = llm_manager->GetLLM("openai");

        // 创建聊天会话管理器
        ChatSessionManager session_manager(llm);

        // 创建代码分析助手
        CodeAnalysisAssistant code_assistant(llm);

        // 示例代码
        std::string example_code = R"(
class Vector {
    int* data;
    int size;
public:
    Vector(int s) {
        data = new int[s];
        size = s;
    }
    ~Vector() {
        delete data;
    }
    int& operator[](int i) {
        return data[i];
    }
};
)";

        // 分析代码
        std::cout << "\n=== Code Analysis ===" << std::endl;
        std::cout << code_assistant.AnalyzeCode(example_code) << std::endl;

        // 生成单元测试
        std::cout << "\n=== Unit Tests ===" << std::endl;
        std::cout << code_assistant.GenerateUnitTests(example_code) << std::endl;

        // 生成文档
        std::cout << "\n=== Documentation ===" << std::endl;
        std::cout << code_assistant.GenerateDocumentation(example_code) << std::endl;

        // 模拟聊天会话
        std::string session_id = "session1";
        
        session_manager.AddMessage(session_id, {
            Role::kSystem,
            "你是一个专业的C++编程助手。"
        });

        session_manager.AddMessage(session_id, {
            Role::kUser,
            "这段代码有什么问题？\n" + example_code
        });

        // 等待一段时间以便看到响应
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // 获取会话历史
        auto history = session_manager.GetSessionHistory(session_id);
        std::cout << "\n=== Chat History ===" << std::endl;
        for (const auto& msg : history) {
            std::cout << "Role: " << static_cast<int>(msg.role) << std::endl;
            std::cout << "Content: " << msg.content << std::endl;
            std::cout << "------------------------" << std::endl;
        }

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
