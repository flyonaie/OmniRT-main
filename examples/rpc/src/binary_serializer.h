// Copyright (c) 2023 OmniRT Authors. All rights reserved.
//
// 二进制序列化器实现
// 用于RPC通信的序列化和反序列化，使用二进制格式而非msgpack

#pragma once

#include "common.h"
#include <cstring>
#include <type_traits>
#include <stdexcept>

namespace omnirt {
namespace rpc {

// 定义辅助命名空间，用于C++11实现参数包展开和元组操作
namespace detail {

// 检查所有类型是否可序列化

template<typename T>
constexpr bool is_binary_serializable() {
    return std::is_trivially_copyable<T>::value;
}

template<typename... Args>
struct AllBinarySerializable;

template<>
struct AllBinarySerializable<> {
    static constexpr bool value = true;
};

template<typename T, typename... Rest>
struct AllBinarySerializable<T, Rest...> {
    static constexpr bool value = is_binary_serializable<T>() && AllBinarySerializable<Rest...>::value;
};

// 计算多个类型的总大小
template<typename... Args>
struct SumSizes;

template<>
struct SumSizes<> {
    static constexpr size_t value = 0;
};

template<typename T, typename... Rest>
struct SumSizes<T, Rest...> {
    static constexpr size_t value = sizeof(T) + SumSizes<Rest...>::value;
};

// 递归序列化剩余参数
template<size_t I, typename... Args>
struct SerializeRestImpl;

template<size_t I>
struct SerializeRestImpl<I> {
    static void apply(uint8_t* /*buffer*/, size_t& /*offset*/) {
        // 终止条件，无需处理
    }
};

template<size_t I, typename T, typename... Rest>
struct SerializeRestImpl<I, T, Rest...> {
    static void apply(uint8_t* buffer, size_t& offset, const T& value, const Rest&... rest) {
        // 序列化当前参数
        std::memcpy(buffer + offset, &value, sizeof(T));
        offset += sizeof(T);
        
        // 递归处理剩余参数
        SerializeRestImpl<I + 1, Rest...>::apply(buffer, offset, rest...);
    }
};

} // namespace detail

/**
 * @brief 二进制序列化器
 * 
 * 使用直接内存复制实现的高性能二进制序列化器，
 * 适用于POD（Plain Old Data）类型和简单结构体。
 * 与msgpack相比，具有更高的性能和更低的开销。
 */
class BinarySerializer : public Serializer {
public:
    /**
     * @brief 默认构造函数
     */
    BinarySerializer() = default;
    
    /**
     * @brief 析构函数
     */
    ~BinarySerializer() override = default;
    
    /**
     * @brief 检查类型是否可通过内存复制安全序列化
     * 
     * @tparam T 要检查的类型
     */
    template<typename T>
    static constexpr bool is_binary_serializable() {
        return std::is_trivially_copyable<T>::value;
    }
    
    // 注意：单参数版本已在下面实现，这里删除以避免函数重载冲突
    
    /**
     * @brief 序列化多个POD类型为一个二进制数据块
     * 
     * @tparam Args POD类型参数包
     * @param args 要序列化的参数列表
     * @return std::vector<uint8_t> 序列化后的二进制数据
     */
    
    // 空参数列表的序列化
    std::vector<uint8_t> Serialize() {
        // 返回空序列
        return std::vector<uint8_t>();
    }
    
    // 单参数序列化（可减少模板实例化个数）
    template<typename T>
    typename std::enable_if<
        is_binary_serializable<T>(), 
        std::vector<uint8_t>>::type
    Serialize(const T& value) {
        // 特殊情况处理：如果类型为0字节，返回空序列
        if (sizeof(T) == 0) {
            return std::vector<uint8_t>();
        }
        
        std::vector<uint8_t> buffer(sizeof(T));
        std::memcpy(buffer.data(), &value, sizeof(T));
        return buffer;
    }
    
    // 多参数的序列化
    template<typename T, typename... Args>
    typename std::enable_if<
        is_binary_serializable<T>() && 
        detail::AllBinarySerializable<Args...>::value && 
        (sizeof...(Args) > 0), // 确保这是多参数版本
        std::vector<uint8_t>>::type
    Serialize(const T& first, const Args&... rest) {
        // 计算总大小
        const size_t total_size = sizeof(T) + detail::SumSizes<Args...>::value;
        
        // 特殊情况处理：如果总大小为0，返回空序列
        if (total_size == 0) {
            return std::vector<uint8_t>();
        }
        
        std::vector<uint8_t> buffer(total_size);
        
        // 先序列化第一个参数
        size_t offset = 0;
        if (sizeof(T) > 0) { // 防止0字节类型
            SerializeToBuffer(buffer.data(), offset, first);
        }
        
        // 递归序列化剩余参数
        detail::SerializeRestImpl<0, Args...>::apply(buffer.data(), offset, rest...);
        
        return buffer;
    }
    
    /**
     * @brief 反序列化二进制数据为指定POD类型
     * 
     * @tparam T 目标POD类型
     * @param data 二进制数据
     * @return T 反序列化后的对象
     */
    template<typename T>
    typename std::enable_if<is_binary_serializable<T>(), T>::type
    Deserialize(const std::vector<uint8_t>& data) {
        std::cout << "DEBUG: Deserialize<T> - 数据大小: " << data.size() << ", 类型大小: " << sizeof(T) << std::endl;
        
        // 特殊情况处理：如果类型大小为0字节
        if (sizeof(T) == 0) {
            std::cout << "DEBUG: Deserialize<T> - 类型大小为0，直接返回空值" << std::endl;
            return T();
        }
        
        // 验证数据大小
        if (data.size() < sizeof(T)) {
            std::cout << "DEBUG: Deserialize<T> - 数据大小不足" << std::endl;
            throw RpcException("反序列化错误：数据大小不足", ErrorCode::SERIALIZATION_ERROR);
        }
        
        // 特殊情况处理：如果数据项为空
        if (data.empty()) {
            std::cout << "DEBUG: Deserialize<T> - 数据为空，返回默认值" << std::endl;
            return T();
        }
        
        // 正常反序列化
        T result;
        std::memcpy(&result, data.data(), sizeof(T));
        std::cout << "DEBUG: Deserialize<T> - 反序列化成功" << std::endl;
        return result;
    }
    
    /**
     * @brief 反序列化二进制数据为指定类型的元组
     * 
     * @tparam Args 元组中的类型列表
     * @param data 二进制数据
     * @return std::tuple<Args...> 反序列化后的元组
     */
    /**
     * @brief 反序列化二进制数据为元组
     * 
     * 特化空参数版本
     * 
     * @return std::tuple<> 空元组
     */
    template<typename... Args>
    typename std::enable_if<sizeof...(Args) == 0, std::tuple<>>::type
    DeserializeAsTuple(const std::vector<uint8_t>& /*data*/) {
        return std::tuple<>();
    }
    
    /**
     * @brief 反序列化二进制数据为元组
     * 
     * 非空参数版本
     * 
     * @return std::tuple<Args...> 反序列化后的元组
     */
    template<typename... Args>
    typename std::enable_if<
        sizeof...(Args) != 0 && 
        detail::AllBinarySerializable<Args...>::value, 
        std::tuple<Args...>>::type
    DeserializeAsTuple(const std::vector<uint8_t>& data) {
        const size_t expected_size = detail::SumSizes<Args...>::value;
        
        // 特殊情况处理：如果预期大小为0，直接返回空元组
        if (expected_size == 0) {
            return std::tuple<Args...>();
        }
        
        // 验证数据大小
        if (data.size() < expected_size) {
            throw RpcException("反序列化元组错误：数据大小不足", ErrorCode::SERIALIZATION_ERROR);
        }
        
        // 创建结果元组
        std::tuple<Args...> result;
        
        // 如果数据不为空，进行反序列化
        if (!data.empty()) {
            size_t offset = 0;
            DeserializeTuple<0, Args...>(result, data.data(), offset);
        }
        
        return result;
    }
    
private:
    /**
     * @brief 将单个值序列化到缓冲区的指定位置
     * 
     * @tparam T 值类型
     * @param buffer 目标缓冲区
     * @param offset 当前偏移量，会更新为序列化后的新偏移量
     * @param value 要序列化的值
     */
    template<typename T>
    void SerializeToBuffer(uint8_t* buffer, size_t& offset, const T& value) {
        std::memcpy(buffer + offset, &value, sizeof(T));
        offset += sizeof(T);
    }
    
    /**
     * @brief 递归反序列化元组的辅助函数
     * 
     * @tparam I 当前元组索引
     * @tparam Args 元组类型列表
     * @param tuple 目标元组
     * @param data 二进制数据
     * @param offset 当前偏移量，会更新为反序列化后的新偏移量
     */
    template<size_t I, typename... Args>
    typename std::enable_if<I == sizeof...(Args), void>::type
    DeserializeTuple(std::tuple<Args...>& /*tuple*/, const uint8_t* /*data*/, size_t& /*offset*/) {
        // 基本情况：所有元素都已处理完毕
    }
    
    /**
     * @brief 递归反序列化元组的辅助函数
     * 
     * @tparam I 当前元组索引
     * @tparam Args 元组类型列表
     * @param tuple 目标元组
     * @param data 二进制数据
     * @param offset 当前偏移量，会更新为反序列化后的新偏移量
     */
    template<size_t I, typename... Args>
    typename std::enable_if<I < sizeof...(Args), void>::type
    DeserializeTuple(std::tuple<Args...>& tuple, const uint8_t* data, size_t& offset) {
        using ElementType = typename std::tuple_element<I, std::tuple<Args...>>::type;
        
        // 安全检查：只有当类型大小大于0时才进行复制
        if (sizeof(ElementType) > 0) {
            std::memcpy(&std::get<I>(tuple), data + offset, sizeof(ElementType));
            offset += sizeof(ElementType);
        }
        
        // 继续递归处理下一个元素
        DeserializeTuple<I + 1, Args...>(tuple, data, offset);
    }
};

} // namespace rpc
} // namespace omnirt
