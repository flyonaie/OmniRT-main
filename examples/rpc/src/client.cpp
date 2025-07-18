// Copyright (c) 2023 OmniRT Authors. All rights reserved.
//
// RPC客户端实现文件

#include "client.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <random>

namespace omnirt {
namespace rpc {

/**
 * @brief 构造函数
 * 
 * @param channel_name RPC通道名称
 * @param config RPC配置参数
 */
Client::Client(const std::string& channel_name, const RpcConfig& config)
    : channel_name_(channel_name),
      config_(config),
      rng_(std::random_device{}()) {
    // 初始化共享内存队列
    if (!InitSharedMemoryQueues()) {
        throw RpcException("无法初始化共享内存队列", ErrorCode::CONNECTION_ERROR);
    }
}

/**
 * @brief 析构函数
 */
Client::~Client() {
    // 停止响应处理线程
    running_.store(false);
    
    if (response_thread_.joinable()) {
        response_thread_.join();
    }
}

/**
 * @brief 等待连接到服务端
 * 
 * @param timeout 超时时间
 * @return true 连接成功
 * @return false 连接超时
 */
bool Client::WaitForConnection(std::chrono::milliseconds timeout) {
    auto start_time = std::chrono::steady_clock::now();
    
    // 尝试发送心跳消息
    while (true) {
        RpcMessage heartbeat;
        heartbeat.header.message_id = GenerateMessageId();
        heartbeat.header.message_type = MessageType::HEARTBEAT;
        heartbeat.method_name = "__heartbeat__";
        heartbeat.header.method_name_len = static_cast<uint32_t>(heartbeat.method_name.size());
        heartbeat.header.payload_size = 0;
        
        if (SendRequest(heartbeat)) {
            // 成功发送心跳，表示已连接
            return true;
        }
        
        // 检查是否超时
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time);
        
        if (elapsed >= timeout) {
            return false;
        }
        
        // 等待一段时间后重试
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

/**
 * @brief 检查是否连接到服务端
 * 
 * @return true 已连接
 * @return false 未连接
 */
bool Client::IsConnected() const {
    return (request_queue_ && request_queue_->GetShmState() != omnirt::common::util::ShmBoundedSpscLockfreeQueue<RpcMessage>::ShmState::NOT_INITIALIZED) &&
           (response_queue_ && response_queue_->GetShmState() != omnirt::common::util::ShmBoundedSpscLockfreeQueue<RpcMessage>::ShmState::NOT_INITIALIZED);
}

/**
 * @brief 生成唯一的消息ID
 * 
 * @return uint64_t 消息ID
 */
uint64_t Client::GenerateMessageId() {
    // 结合原子计数器和随机数，确保消息ID的唯一性
    uint64_t id = next_message_id_.fetch_add(1, std::memory_order_relaxed);
    uint64_t random = rng_();
    
    return (id << 32) | (random & 0xFFFFFFFF);
}

/**
 * @brief 发送RPC请求
 * 
 * @param request 请求消息
 * @return true 发送成功
 * @return false 发送失败
 */
bool Client::SendRequest(const RpcMessage& request) {
    if (!request_queue_) {
        return false;
    }
    
    // 尝试将请求放入请求队列
    return request_queue_->Enqueue(request);
}

/**
 * @brief 等待RPC响应
 * 
 * @param message_id 消息ID
 * @param timeout 超时时间
 * @param response 用于存储响应的引用
 * @return true 接收到响应
 * @return false 等待超时
 */
bool Client::WaitForResponse(uint64_t message_id, std::chrono::milliseconds timeout, RpcMessage& response) {
    std::unique_lock<std::mutex> lock(pending_mutex_);
    
    // 检查是否已经收到响应
    auto it = pending_responses_.find(message_id);
    if (it != pending_responses_.end()) {
        response = it->second;
        pending_responses_.erase(it);
        return true;
    }
    
    // 等待响应
    auto pred = [this, message_id]() {
        return pending_responses_.find(message_id) != pending_responses_.end();
    };
    
    bool result;
    if (timeout.count() > 0) {
        result = pending_cv_.wait_for(lock, timeout, pred);
    } else {
        pending_cv_.wait(lock, pred);
        result = true;
    }
    
    if (result) {
        // 获取响应
        auto it = pending_responses_.find(message_id);
        if (it != pending_responses_.end()) {
            response = it->second;
            pending_responses_.erase(it);
            return true;
        }
    }
    
    return false;
}

/**
 * @brief 响应处理线程
 */
void Client::ResponseHandler() {
    RpcMessage response;
    
    while (running_.load()) {
        // 尝试从响应队列中取出响应
        if (response_queue_->Dequeue(&response)) {
            // 处理响应
            std::lock_guard<std::mutex> lock(pending_mutex_);
            
            // 保存响应并通知等待线程
            pending_responses_[response.header.message_id] = response;
            pending_cv_.notify_all();
        } else {
            // 没有响应，短暂休眠避免CPU占用过高
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
bool Client::InitSharedMemoryQueues() {
    try {
        // 初始化请求队列（客户端为生产者）
        request_queue_ = std::make_unique<RequestQueue>();
        std::string request_queue_name = "/omnirt_rpc_req_" + channel_name_;
        if (!request_queue_->Init(request_queue_name, config_.request_queue_size, true, false)) {
            std::cerr << "错误: 无法初始化请求队列" << std::endl;
            return false;
        }
        
        // 初始化响应队列（客户端为消费者）
        response_queue_ = std::make_unique<ResponseQueue>();
        std::string response_queue_name = "/omnirt_rpc_resp_" + channel_name_;
        if (!response_queue_->Init(response_queue_name, config_.response_queue_size, true, false)) {
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
