// Copyright (c) 2023 OmniRT Authors. All rights reserved.
//
// RPC客户端头文件
// 实现RPC方法调用

#pragma once

#include "common.h"
#include "binary_serializer.h"
#include "../../../src/common/util/shm_bounded_spsc_lockfree_queue.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <future>
#include <atomic>
#include <random>
#include <chrono>

namespace omnirt {
namespace rpc {

/**
 * @brief RPC调用结果类
 * 
 * 用于存储异步RPC调用的结果
 * 
 * @tparam R 返回值类型
 */
template<typename R>
class RpcResult {
public:
    /**
     * @brief 构造函数
     */
    RpcResult() : ready_(false), error_(ErrorCode::SUCCESS) {}
    
    /**
     * @brief 设置结果
     * 
     * @param result 结果值
     */
    void SetResult(const R& result) {
        std::lock_guard<std::mutex> lock(mutex_);
        result_ = result;
        ready_ = true;
        cv_.notify_all();
    }
    
    /**
     * @brief 设置错误
     * 
     * @param error 错误码
     * @param error_message 错误消息
     */
    void SetError(ErrorCode error, const std::string& error_message) {
        std::lock_guard<std::mutex> lock(mutex_);
        error_ = error;
        error_message_ = error_message;
        ready_ = true;
        cv_.notify_all();
    }
    
    /**
     * @brief 等待结果
     * 
     * @param timeout 超时时间
     * @return true 结果已就绪
     * @return false 等待超时
     */
    bool Wait(std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (timeout.count() == 0) {
            cv_.wait(lock, [this]{ return ready_; });
            return true;
        } else {
            return cv_.wait_for(lock, timeout, [this]{ return ready_; });
        }
    }
    
    /**
     * @brief 获取结果
     * 
     * @return R 结果值
     * @throws RpcException 如果发生错误或结果未就绪
     */
    R GetResult() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ready_) {
            throw RpcException("结果未就绪", ErrorCode::TIMEOUT);
        }
        
        if (error_ != ErrorCode::SUCCESS) {
            throw RpcException(error_message_, error_);
        }
        
        return result_;
    }
    
    /**
     * @brief 检查是否就绪
     * 
     * @return true 结果已就绪
     * @return false 结果未就绪
     */
    bool IsReady() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return ready_;
    }
    
    /**
     * @brief 检查是否有错误
     * 
     * @return true 有错误
     * @return false 无错误
     */
    bool HasError() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return ready_ && (error_ != ErrorCode::SUCCESS);
    }
    
    /**
     * @brief 获取错误码
     * 
     * @return ErrorCode 错误码
     */
    ErrorCode GetError() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return error_;
    }
    
    /**
     * @brief 获取错误消息
     * 
     * @return std::string 错误消息
     */
    std::string GetErrorMessage() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return error_message_;
    }
    
private:
    mutable std::mutex mutex_;          ///< 互斥锁
    std::condition_variable cv_;        ///< 条件变量
    bool ready_;                        ///< 就绪标志
    R result_;                          ///< 结果值
    ErrorCode error_;                   ///< 错误码
    std::string error_message_;         ///< 错误消息
};

/**
 * @brief void类型的RPC调用结果特化
 */
template<>
class RpcResult<void> {
public:
    /**
     * @brief 构造函数
     */
    RpcResult() : ready_(false), error_(ErrorCode::SUCCESS) {}
    
    /**
     * @brief 设置结果
     */
    void SetResult() {
        std::lock_guard<std::mutex> lock(mutex_);
        ready_ = true;
        cv_.notify_all();
    }
    
    /**
     * @brief 设置错误
     * 
     * @param error 错误码
     * @param error_message 错误消息
     */
    void SetError(ErrorCode error, const std::string& error_message) {
        std::lock_guard<std::mutex> lock(mutex_);
        error_ = error;
        error_message_ = error_message;
        ready_ = true;
        cv_.notify_all();
    }
    
    /**
     * @brief 等待结果
     * 
     * @param timeout 超时时间
     * @return true 结果已就绪
     * @return false 等待超时
     */
    bool Wait(std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (timeout.count() == 0) {
            cv_.wait(lock, [this]{ return ready_; });
            return true;
        } else {
            return cv_.wait_for(lock, timeout, [this]{ return ready_; });
        }
    }
    
    /**
     * @brief 获取结果
     * 
     * @throws RpcException 如果发生错误或结果未就绪
     */
    void GetResult() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ready_) {
            throw RpcException("结果未就绪", ErrorCode::TIMEOUT);
        }
        
        if (error_ != ErrorCode::SUCCESS) {
            throw RpcException(error_message_, error_);
        }
    }
    
    /**
     * @brief 检查是否就绪
     * 
     * @return true 结果已就绪
     * @return false 结果未就绪
     */
    bool IsReady() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return ready_;
    }
    
    /**
     * @brief 检查是否有错误
     * 
     * @return true 有错误
     * @return false 无错误
     */
    bool HasError() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return ready_ && (error_ != ErrorCode::SUCCESS);
    }
    
    /**
     * @brief 获取错误码
     * 
     * @return ErrorCode 错误码
     */
    ErrorCode GetError() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return error_;
    }
    
    /**
     * @brief 获取错误消息
     * 
     * @return std::string 错误消息
     */
    std::string GetErrorMessage() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return error_message_;
    }
    
private:
    mutable std::mutex mutex_;          ///< 互斥锁
    std::condition_variable cv_;        ///< 条件变量
    bool ready_;                        ///< 就绪标志
    ErrorCode error_;                   ///< 错误码
    std::string error_message_;         ///< 错误消息
};

/**
 * @brief RPC客户端类
 * 
 * 基于共享内存的RPC客户端实现，负责发送RPC请求并处理响应
 */
class Client {
public:
    /**
     * @brief 构造函数
     * 
     * @param channel_name RPC通道名称，用于标识共享内存
     * @param config RPC配置参数
     */
    explicit Client(const std::string& channel_name, const RpcConfig& config = RpcConfig());
    
    /**
     * @brief 析构函数
     */
    ~Client();
    
    /**
     * @brief 禁用拷贝构造函数
     */
    Client(const Client&) = delete;
    
    /**
     * @brief 禁用赋值运算符
     */
    Client& operator=(const Client&) = delete;
    
    /**
     * @brief 等待连接到服务端
     * 
     * @param timeout 超时时间
     * @return true 连接成功
     * @return false 连接超时
     */
    bool WaitForConnection(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    
    /**
     * @brief 同步调用RPC方法
     * 
     * @tparam R 返回类型
     * @tparam Args 参数类型列表
     * @param method_name 方法名
     * @param args 参数列表
     * @return R 返回值
     * @throws RpcException 如果调用失败
     */
    template<typename R, typename... Args>
    R Call(const std::string& method_name, Args... args);
    
    /**
     * @brief 异步调用RPC方法
     * 
     * @tparam R 返回类型
     * @tparam Args 参数类型列表
     * @param method_name 方法名
     * @param args 参数列表
     * @return std::shared_ptr<RpcResult<R>> 返回结果的future
     */
    template<typename R, typename... Args>
    std::shared_ptr<RpcResult<R>> AsyncCall(const std::string& method_name, Args... args);
    
    /**
     * @brief 检查是否连接到服务端
     * 
     * @return true 已连接
     * @return false 未连接
     */
    bool IsConnected() const;
    
private:
    /**
     * @brief 生成唯一的消息ID
     * 
     * @return uint64_t 消息ID
     */
    uint64_t GenerateMessageId();
    
    /**
     * @brief 发送RPC请求
     * 
     * @param request 请求消息
     * @return true 发送成功
     * @return false 发送失败
     */
    bool SendRequest(const RpcMessage& request);
    
    /**
     * @brief 等待RPC响应
     * 
     * @param message_id 消息ID
     * @param timeout 超时时间
     * @param response 用于存储响应的引用
     * @return true 接收到响应
     * @return false 等待超时
     */
    bool WaitForResponse(uint64_t message_id, std::chrono::milliseconds timeout, RpcMessage& response);
    
    /**
     * @brief 响应处理线程
     */
    void ResponseHandler();
    
    /**
     * @brief 初始化共享内存队列
     * 
     * @return true 初始化成功
     * @return false 初始化失败
     */
    bool InitSharedMemoryQueues();
    
private:
    std::string channel_name_;          ///< RPC通道名称
    RpcConfig config_;                  ///< RPC配置
    
    using RequestQueue = omnirt::common::util::ShmBoundedSpscLockfreeQueue<RpcMessage>;
    using ResponseQueue = omnirt::common::util::ShmBoundedSpscLockfreeQueue<RpcMessage>;
    
    std::unique_ptr<RequestQueue> request_queue_;   ///< 请求队列
    std::unique_ptr<ResponseQueue> response_queue_;  ///< 响应队列
    
    std::atomic<bool> running_{false};  ///< 运行标志
    std::thread response_thread_;       ///< 响应处理线程
    
    std::mutex pending_mutex_;          ///< 挂起请求的互斥锁
    std::condition_variable pending_cv_; ///< 挂起请求的条件变量
    std::unordered_map<uint64_t, RpcMessage> pending_responses_; ///< 挂起的响应
    
    std::atomic<uint64_t> next_message_id_{1}; ///< 下一个消息ID
    std::mt19937_64 rng_;               ///< 随机数生成器（用于消息ID）
    
    BinarySerializer serializer_;       ///< 二进制序列化器
    
    /**
     * @brief 处理非void类型的响应
     * 
     * @tparam R 响应类型
     * @param response RPC响应消息
     * @param result RPC返回结果
     * @param serializer 序列化器
     */
    template<typename R>
    typename std::enable_if<!std::is_void<R>::value>::type
    process_response(const RpcMessage& response, std::shared_ptr<RpcResult<R>> result, BinarySerializer& serializer) {
        if (!response.payload.empty()) {
            R ret_val = serializer.Deserialize<R>(response.payload);
            result->SetResult(ret_val);
        } else {
            result->SetError(ErrorCode::SERIALIZATION_ERROR, "响应负载为空");
        }
    }
    
    /**
     * @brief 处理void类型的响应
     * 
     * @param response RPC响应消息
     * @param result RPC返回结果
     * @param serializer 序列化器
     */
    template<typename R>
    typename std::enable_if<std::is_void<R>::value>::type
    process_response(const RpcMessage& /*response*/, std::shared_ptr<RpcResult<R>> result, BinarySerializer& /*serializer*/) {
        result->SetResult();
    }
};

// 模板函数实现

template<typename R, typename... Args>
R Client::Call(const std::string& method_name, Args... args) {
    try {
        std::cout << "DEBUG: Call - 正在调用方法: " << method_name << std::endl;
        auto result = AsyncCall<R>(method_name, std::forward<Args>(args)...);
        std::cout << "DEBUG: Call - AsyncCall返回成功, 等待结果" << std::endl;
        
        // 等待结果
        if (!result->Wait(config_.default_timeout)) {
            std::cout << "DEBUG: Call - 等待超时" << std::endl;
            throw RpcException("RPC调用超时: " + method_name, ErrorCode::TIMEOUT);
        }
        
        std::cout << "DEBUG: Call - 成功获取结果, 准备返回" << std::endl;
        // 返回结果
        return result->GetResult();
    } catch (const std::exception& e) {
        std::cout << "DEBUG: Call - 捕获异常: " << e.what() << std::endl;
        throw; // 重新抛出异常
    }
}

template<typename R, typename... Args>
std::shared_ptr<RpcResult<R>> Client::AsyncCall(const std::string& method_name, Args... args) {
    std::cout << "DEBUG: AsyncCall - 开始异步调用方法: " << method_name << std::endl;

    // 创建结果对象
    auto result = std::make_shared<RpcResult<R>>();
    
    try {
        // 序列化参数
        std::cout << "DEBUG: AsyncCall - 序列化参数" << std::endl;
        std::vector<uint8_t> payload = serializer_.Serialize(std::forward<Args>(args)...);
        std::cout << "DEBUG: AsyncCall - 参数序列化成功, 负载大小: " << payload.size() << " 字节" << std::endl;
        
        // 创建请求消息
        RpcMessage request;
        request.header.message_id = GenerateMessageId();
        request.header.message_type = MessageType::REQUEST;
        request.method_name = method_name;
        request.header.method_name_len = static_cast<uint32_t>(method_name.size());
        request.payload = std::move(payload);
        request.header.payload_size = static_cast<uint32_t>(request.payload.size());
        request.header.error_code = ErrorCode::SUCCESS;
        std::cout << "DEBUG: AsyncCall - 请求消息创建成功, message_id: " << request.header.message_id << std::endl;
        
        // 发送请求
        std::cout << "DEBUG: AsyncCall - 尝试发送请求" << std::endl;
        if (!SendRequest(request)) {
            std::cout << "DEBUG: AsyncCall - 发送请求失败" << std::endl;
            result->SetError(ErrorCode::CONNECTION_ERROR, "无法发送RPC请求");
            return result;
        }
        std::cout << "DEBUG: AsyncCall - 请求发送成功" << std::endl;
        
        // 启动响应处理线程（如果尚未启动）
        if (!running_.load()) {
            std::cout << "DEBUG: AsyncCall - 启动响应处理线程" << std::endl;
            running_.store(true);
            response_thread_ = std::thread([this]() {
                ResponseHandler();
            });
        }
        
        // 创建lamda来处理响应
        std::cout << "DEBUG: AsyncCall - 创建响应处理线程" << std::endl;
        auto message_id = request.header.message_id;
        std::thread([this, message_id, result, method_name]() {
            std::cout << "DEBUG: AsyncCall - 响应处理线程开始执行, 等待响应 (message_id: " << message_id << ")" << std::endl;
            RpcMessage response;
            
            // 等待响应
            std::cout << "DEBUG: AsyncCall - 开始等待响应: " << method_name << std::endl;
            if (WaitForResponse(message_id, config_.default_timeout, response)) {
                std::cout << "DEBUG: AsyncCall - 收到响应, 错误代码: " << static_cast<int>(response.header.error_code) << std::endl;
                if (response.header.error_code == ErrorCode::SUCCESS) {
                    try {
                        // 反序列化结果
                        std::cout << "DEBUG: AsyncCall - 尝试反序列化结果, 负载大小: " << response.payload.size() << " 字节" << std::endl;
                        // C++11不支持if constexpr，改用类型特殊化
                        process_response<R>(response, result, serializer_);
                        std::cout << "DEBUG: AsyncCall - 反序列化成功" << std::endl;
                    } catch (const std::exception& e) {
                        std::cout << "DEBUG: AsyncCall - 反序列化错误: " << e.what() << std::endl;
                        result->SetError(ErrorCode::SERIALIZATION_ERROR, std::string("反序列化错误: ") + e.what());
                    }
                } else {
                    std::cout << "DEBUG: AsyncCall - 服务器返回错误" << std::endl;
                    // 处理错误响应
                    std::string error_message;
                    if (!response.payload.empty()) {
                        error_message.resize(response.payload.size());
                        std::memcpy(&error_message[0], response.payload.data(), response.payload.size());
                    } else {
                        error_message = "未知RPC错误";
                    }
                    result->SetError(response.header.error_code, error_message);
                }
            } else {
                // 超时
                result->SetError(ErrorCode::TIMEOUT, "RPC调用超时: " + response.method_name);
            }
        }).detach();
        
    } catch (const std::exception& e) {
        result->SetError(ErrorCode::SERIALIZATION_ERROR, std::string("序列化错误: ") + e.what());
    }
    
    return result;
}

} // namespace rpc
} // namespace omnirt
