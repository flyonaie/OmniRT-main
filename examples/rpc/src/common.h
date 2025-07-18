// Copyright (c) 2023 OmniRT Authors. All rights reserved.
//
// RPC框架公共定义头文件
// 包含RPC通信所需的公共结构、枚举和常量定义

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <chrono>
#include <stdexcept>
#include <type_traits>
#include <tuple>
#include <atomic>

// 实现C++11下的tuple基础工具函数

// 序列生成
// 类似于C++14的index_sequence的简化版本
template<std::size_t...> struct index_sequence {};

template<std::size_t N, std::size_t... Idx>
struct make_index_sequence_helper : make_index_sequence_helper<N-1, N-1, Idx...> {};

template<std::size_t... Idx>
struct make_index_sequence_helper<0, Idx...> {
    using type = index_sequence<Idx...>;
};

template<std::size_t N>
using make_index_sequence = typename make_index_sequence_helper<N>::type;

// 执行函数，直接展开元组
// 这种实现更简单稳定，适用于大多数情况
template<typename F, typename Tuple, std::size_t... Idx>
static auto tuple_invoke_impl(F&& f, Tuple&& t, index_sequence<Idx...>)
  -> decltype(std::forward<F>(f)(std::get<Idx>(std::forward<Tuple>(t))...))
{
    return std::forward<F>(f)(std::get<Idx>(std::forward<Tuple>(t))...);
}

// 简化的tuple_invoke API
template<typename F, typename Tuple>
auto tuple_invoke(F&& f, Tuple&& t)
  -> decltype(tuple_invoke_impl(
         std::forward<F>(f),
         std::forward<Tuple>(t),
         make_index_sequence<std::tuple_size<typename std::decay<Tuple>::type>::value>()))
{
    return tuple_invoke_impl(
        std::forward<F>(f),
        std::forward<Tuple>(t),
        make_index_sequence<std::tuple_size<typename std::decay<Tuple>::type>::value>());
}

// 为C++11实现std::make_unique函数
#if __cplusplus == 201103L
namespace std {
    template<typename T, typename... Args>
    std::unique_ptr<T> make_unique(Args&&... args) {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
}
#endif

namespace omnirt {
namespace rpc {

/**
 * @brief RPC通信配置参数
 * 
 * 用于配置RPC通信的参数，包括队列大小、超时设置等
 */
struct RpcConfig {
    uint32_t request_queue_size = 1024;   ///< 请求队列大小
    uint32_t response_queue_size = 1024;  ///< 响应队列大小
    uint32_t max_message_size = 4096;     ///< 最大消息大小(字节)
    std::chrono::milliseconds default_timeout{5000};  ///< 默认调用超时时间
    bool auto_reconnect = true;           ///< 是否自动重连
};

/**
 * @brief RPC消息类型枚举
 */
enum class MessageType : uint8_t {
    REQUEST = 1,    ///< RPC请求消息
    RESPONSE = 2,   ///< RPC响应消息
    HEARTBEAT = 3,  ///< 心跳消息
    ERROR = 4       ///< 错误消息
};

/**
 * @brief RPC错误码枚举
 */
enum class ErrorCode : uint32_t {
    SUCCESS = 0,           ///< 成功
    METHOD_NOT_FOUND = 1,  ///< 方法未找到
    INVALID_ARGS = 2,      ///< 参数无效
    EXECUTION_ERROR = 3,   ///< 执行错误
    TIMEOUT = 4,           ///< 超时
    CONNECTION_ERROR = 5,  ///< 连接错误
    SERIALIZATION_ERROR = 6, ///< 序列化错误
    UNKNOWN_ERROR = 999    ///< 未知错误
};

/**
 * @brief RPC请求头部结构
 * 
 * 包含RPC请求的元数据，如请求ID、方法名等
 */
struct RpcHeader {
    uint64_t message_id;      ///< 唯一消息ID
    MessageType message_type; ///< 消息类型
    uint32_t method_name_len; ///< 方法名长度
    uint32_t payload_size;    ///< 负载大小
    ErrorCode error_code;     ///< 错误码
};

/**
 * @brief RPC消息结构
 * 
 * 完整的RPC消息，包含头部和数据部分
 */
struct RpcMessage {
    RpcHeader header;              ///< 消息头部
    std::string method_name;       ///< 方法名
    std::vector<uint8_t> payload;  ///< 序列化后的负载数据
};

/**
 * @brief RPC异常类
 */
class RpcException : public std::runtime_error {
public:
    /**
     * @brief 构造函数
     * 
     * @param message 错误消息
     * @param code 错误码
     */
    RpcException(const std::string& message, ErrorCode code)
        : std::runtime_error(message), error_code_(code) {}
    
    /**
     * @brief 获取错误码
     * 
     * @return 错误码
     */
    ErrorCode GetErrorCode() const { return error_code_; }
    
private:
    ErrorCode error_code_; ///< 错误码
};

/**
 * @brief 序列化器接口
 * 
 * 定义序列化和反序列化的接口，可以实现不同的序列化方案
 */
class Serializer {
public:
    virtual ~Serializer() = default;
    
    /**
     * @brief 序列化数据为二进制
     * 
     * @tparam Args 参数类型列表
     * @param args 要序列化的参数列表
     * @return std::vector<uint8_t> 序列化后的二进制数据
     */
    template<typename... Args>
    std::vector<uint8_t> Serialize(const Args&... args);
    
    /**
     * @brief 反序列化二进制数据为指定类型
     * 
     * @tparam T 目标类型
     * @param data 二进制数据
     * @return T 反序列化后的对象
     */
    template<typename T>
    T Deserialize(const std::vector<uint8_t>& data);
    
    /**
     * @brief 反序列化二进制数据为指定类型的元组
     * 
     * @tparam Args 元组中的类型列表
     * @param data 二进制数据
     * @return std::tuple<Args...> 反序列化后的元组
     */
    template<typename... Args>
    std::tuple<Args...> DeserializeAsTuple(const std::vector<uint8_t>& data);
};

} // namespace rpc
} // namespace omnirt
