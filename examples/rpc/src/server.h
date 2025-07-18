// Copyright (c) 2023 OmniRT Authors. All rights reserved.
//
// RPC服务端头文件
// 实现RPC服务注册和请求处理

#pragma once

#include "common.h"
#include "binary_serializer.h"
#include "../../../src/common/util/shm_bounded_spsc_lockfree_queue.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <functional>
#include <any>
#include <atomic>

namespace omnirt {
namespace rpc {

/**
 * @brief RPC服务端类
 * 
 * 基于共享内存的RPC服务器实现，负责管理RPC方法并处理客户端请求
 */
class Server {
public:
    /**
     * @brief 构造函数
     * 
     * @param channel_name RPC通道名称，用于标识共享内存
     * @param config RPC配置参数
     */
    explicit Server(const std::string& channel_name, const RpcConfig& config = RpcConfig());
    
    /**
     * @brief 析构函数
     */
    ~Server();
    
    /**
     * @brief 禁用拷贝构造函数
     */
    Server(const Server&) = delete;
    
    /**
     * @brief 禁用赋值运算符
     */
    Server& operator=(const Server&) = delete;
    
    /**
     * @brief 注册RPC方法
     * 
     * @tparam Func 函数类型
     * @param method_name 方法名
     * @param func 函数指针或可调用对象
     */
    /**
     * @brief 注册RPC方法（使用函数指针）
     * 
     * @tparam R 返回类型
     * @tparam Args 参数类型列表
     * @param method_name 方法名
     * @param func 函数指针
     */
    template<typename R, typename... Args>
    void Bind(const std::string& method_name, R (*func)(Args...));
    
    /**
     * @brief 注册RPC方法（使用std::function）
     * 
     * @tparam R 返回类型
     * @tparam Args 参数类型列表
     * @param method_name 方法名
     * @param func 函数对象
     */
    template<typename R, typename... Args>
    void Bind(const std::string& method_name, std::function<R(Args...)> func);
    
    /**
     * @brief 开始运行服务器
     * 
     * 阻塞当前线程，循环处理客户端请求
     */
    void Run();
    
    /**
     * @brief 在后台线程运行服务器
     * 
     * 非阻塞方式启动服务器
     */
    void RunInBackground();
    
    /**
     * @brief 停止服务器
     * 
     * 停止处理客户端请求
     */
    void Stop();
    
    /**
     * @brief 等待服务器停止
     */
    void WaitForStop();
    
private:
    /**
     * @brief 方法处理器基类
     */
    class MethodHandlerBase {
    public:
        virtual ~MethodHandlerBase() = default;
        
        /**
         * @brief 调用方法
         * 
         * @param payload 参数数据
         * @return std::vector<uint8_t> 返回值数据
         */
        virtual std::vector<uint8_t> Invoke(const std::vector<uint8_t>& payload) = 0;
    };
    
    /**
     * @brief 方法处理器模板实现
     * 
     * @tparam R 返回类型
     * @tparam Args 参数类型列表
     */
    template<typename R, typename... Args>
    class MethodHandler : public MethodHandlerBase {
    public:
        /**
         * @brief 构造函数
         * 
         * @param func 函数对象
         */
        explicit MethodHandler(std::function<R(Args...)> func)
            : func_(std::move(func)) {}
        
        /**
         * @brief 调用方法
         * 
         * @param payload 参数数据
         * @return std::vector<uint8_t> 返回值数据
         */
        std::vector<uint8_t> Invoke(const std::vector<uint8_t>& payload) override {
            BinarySerializer serializer;
            
            // 反序列化参数
            auto args = serializer.DeserializeAsTuple<Args...>(payload);
            
            // 调用函数
            R result = tuple_invoke(func_, args);
            
            // 序列化返回值
            return serializer.Serialize(result);
        }
        
    private:
        std::function<R(Args...)> func_; ///< 函数对象
    };
    
    /**
     * @brief 无返回值方法处理器特化
     * 
     * @tparam Args 参数类型列表
     */
    template<typename... Args>
    class MethodHandler<void, Args...> : public MethodHandlerBase {
    public:
        /**
         * @brief 构造函数
         * 
         * @param func 函数对象
         */
        explicit MethodHandler(std::function<void(Args...)> func)
            : func_(std::move(func)) {}
        
        /**
         * @brief 调用方法
         * 
         * @param payload 参数数据
         * @return std::vector<uint8_t> 空数据
         */
        std::vector<uint8_t> Invoke(const std::vector<uint8_t>& payload) override {
            BinarySerializer serializer;
            
            // 反序列化参数
            auto args = serializer.DeserializeAsTuple<Args...>(payload);
            
            // 调用函数
            tuple_invoke(func_, args);
            
            // 返回空数据
            return {};
        }
        
    private:
        std::function<void(Args...)> func_; ///< 函数对象
    };
    
    /**
     * @brief 处理RPC请求
     * 
     * @param request 请求消息
     * @return RpcMessage 响应消息
     */
    RpcMessage ProcessRequest(const RpcMessage& request);
    
    /**
     * @brief 处理请求循环
     */
    void ProcessLoop();
    
    /**
     * @brief 初始化共享内存队列
     * 
     * @return true 初始化成功
     * @return false 初始化失败
     */
    bool InitSharedMemoryQueues();
    
private:
    std::string channel_name_;    ///< RPC通道名称
    RpcConfig config_;            ///< RPC配置
    
    std::unordered_map<std::string, std::unique_ptr<MethodHandlerBase>> method_handlers_; ///< 方法处理器映射
    std::mutex handlers_mutex_;   ///< 方法处理器映射的互斥锁
    
    using RequestQueue = omnirt::common::util::ShmBoundedSpscLockfreeQueue<RpcMessage>;
    using ResponseQueue = omnirt::common::util::ShmBoundedSpscLockfreeQueue<RpcMessage>;
    
    std::unique_ptr<RequestQueue> request_queue_;  ///< 请求队列
    std::unique_ptr<ResponseQueue> response_queue_; ///< 响应队列
    
    std::atomic<bool> running_{false}; ///< 运行标志
    std::thread worker_thread_;       ///< 工作线程
    
    BinarySerializer serializer_; ///< 二进制序列化器
};

// 模板函数实现

/**
 * @brief 注册RPC方法（函数指针）
 * 
 * @tparam R 返回类型
 * @tparam Args 参数类型列表
 * @param method_name 方法名
 * @param func 函数指针
 */
template<typename R, typename... Args>
void Server::Bind(const std::string& method_name, R (*func)(Args...)) {
    // 将函数指针转换为std::function并调用相应的Bind函数
    std::function<R(Args...)> std_func = func;
    Bind(method_name, std_func);
}

/**
 * @brief 注册RPC方法（std::function）
 * 
 * @tparam R 返回类型
 * @tparam Args 参数类型列表
 * @param method_name 方法名
 * @param func 函数对象
 */
template<typename R, typename... Args>
void Server::Bind(const std::string& method_name, std::function<R(Args...)> func) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    method_handlers_[method_name] = std::unique_ptr<MethodHandlerBase>(new MethodHandler<R, Args...>(std::move(func)));
}

} // namespace rpc
} // namespace omnirt
