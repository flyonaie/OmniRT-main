// Copyright (c) 2023 OmniRT Authors. All rights reserved.
//
// RPC服务端实现文件

#include "server.h"
#include <iostream>
#include <sstream>

namespace omnirt {
namespace rpc {

/**
 * @brief 构造函数
 * 
 * @param channel_name RPC通道名称
 * @param config RPC配置参数
 */
Server::Server(const std::string& channel_name, const RpcConfig& config)
    : channel_name_(channel_name),
      config_(config) {
    // 初始化共享内存队列
    if (!InitSharedMemoryQueues()) {
        throw RpcException("无法初始化共享内存队列", ErrorCode::CONNECTION_ERROR);
    }
}

/**
 * @brief 析构函数
 */
Server::~Server() {
    Stop();
    WaitForStop();
}

/**
 * @brief 开始运行服务器
 */
void Server::Run() {
    if (running_.exchange(true)) {
        // 已经在运行
        return;
    }
    
    ProcessLoop();
}

/**
 * @brief 在后台线程运行服务器
 */
void Server::RunInBackground() {
    if (running_.exchange(true)) {
        // 已经在运行
        return;
    }
    
    worker_thread_ = std::thread([this]() {
        ProcessLoop();
    });
}

/**
 * @brief 停止服务器
 */
void Server::Stop() {
    running_.store(false);
}

/**
 * @brief 等待服务器停止
 */
void Server::WaitForStop() {
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

/**
 * @brief 处理RPC请求
 * 
 * @param request 请求消息
 * @return RpcMessage 响应消息
 */
RpcMessage Server::ProcessRequest(const RpcMessage& request) {
    RpcMessage response;
    response.header.message_id = request.header.message_id;
    response.header.message_type = MessageType::RESPONSE;
    response.method_name = request.method_name;
    
    try {
        // 查找方法处理器
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        auto it = method_handlers_.find(request.method_name);
        
        if (it == method_handlers_.end()) {
            // 方法未找到
            response.header.error_code = ErrorCode::METHOD_NOT_FOUND;
            std::string error_msg = "未找到方法: " + request.method_name;
            response.payload.resize(error_msg.size());
            std::memcpy(response.payload.data(), error_msg.data(), error_msg.size());
        } else {
            // 调用方法处理器
            response.payload = it->second->Invoke(request.payload);
            response.header.error_code = ErrorCode::SUCCESS;
        }
    } catch (const RpcException& e) {
        // RPC异常
        response.header.error_code = e.GetErrorCode();
        std::string error_msg = e.what();
        response.payload.resize(error_msg.size());
        std::memcpy(response.payload.data(), error_msg.data(), error_msg.size());
    } catch (const std::exception& e) {
        // 其他异常
        response.header.error_code = ErrorCode::EXECUTION_ERROR;
        std::string error_msg = e.what();
        response.payload.resize(error_msg.size());
        std::memcpy(response.payload.data(), error_msg.data(), error_msg.size());
    } catch (...) {
        // 未知异常
        response.header.error_code = ErrorCode::UNKNOWN_ERROR;
        std::string error_msg = "未知错误";
        response.payload.resize(error_msg.size());
        std::memcpy(response.payload.data(), error_msg.data(), error_msg.size());
    }
    
    response.header.payload_size = static_cast<uint32_t>(response.payload.size());
    
    return response;
}

/**
 * @brief 处理请求循环
 */
void Server::ProcessLoop() {
    RpcMessage request;
    
    while (running_.load()) {
        // 尝试从请求队列中取出请求
        if (request_queue_->Dequeue(&request)) {
            // 处理请求
            RpcMessage response = ProcessRequest(request);
            
            // 将响应发送到响应队列
            if (!response_queue_->Enqueue(response)) {
                std::cerr << "警告: 无法将响应入队，响应队列可能已满" << std::endl;
            }
        } else {
            // 没有请求，短暂休眠避免CPU占用过高
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

/**
 * @brief 初始化共享内存队列
 * 
 * @return true 初始化成功
 * @return false 初始化失败
 */
bool Server::InitSharedMemoryQueues() {
    try {
        // 初始化请求队列（服务端为消费者）
        request_queue_ = std::make_unique<RequestQueue>();
        std::string request_queue_name = "/omnirt_rpc_req_" + channel_name_;
        if (!request_queue_->Init(request_queue_name, config_.request_queue_size, true, true)) {
            std::cerr << "错误: 无法初始化请求队列" << std::endl;
            return false;
        }
        
        // 初始化响应队列（服务端为生产者）
        response_queue_ = std::make_unique<ResponseQueue>();
        std::string response_queue_name = "/omnirt_rpc_resp_" + channel_name_;
        if (!response_queue_->Init(response_queue_name, config_.response_queue_size, true, true)) {
            std::cerr << "错误: 无法初始化响应队列" << std::endl;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "初始化共享内存队列时发生异常: " << e.what() << std::endl;
        return false;
    }
}

} // namespace rpc
} // namespace omnirt
