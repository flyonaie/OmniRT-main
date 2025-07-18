// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

/**
 * @file channel_registry.h
 * @brief 通道注册表，管理发布/订阅关系和消息路由
 * @details 该类负责：
 *          - 管理订阅者的注册和回调函数
 *          - 管理发布者的类型信息
 *          - 提供消息路由和查找机制
 *          - 维护主题索引和类型缓存
 */

#pragma once

#include <memory>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "core/channel/channel_msg_wrapper.h"
#include "util/log_util.h"

namespace aimrt::runtime::core::channel {

/**
 * @brief 订阅者回调函数类型
 * @param msg_wrapper 消息包装器，包含消息内容和上下文信息
 * @param done_callback 消息处理完成后的回调函数
 */
using SubscriberCallback = std::function<void(MsgWrapper&, std::function<void()>&&)>;

/**
 * @brief 订阅包装器，包含订阅相关的所有信息
 */
struct SubscribeWrapper {
  TopicInfo info;                                              ///< 主题信息
  std::unordered_set<std::string> require_cache_serialization_types;  ///< 需要缓存序列化的类型集合
  SubscriberCallback callback;                                 ///< 订阅者的回调函数
};

/**
 * @brief 发布类型包装器，包含发布者相关的类型信息
 */
struct PublishTypeWrapper {
  TopicInfo info;                                              ///< 主题信息
  std::unordered_set<std::string> require_cache_serialization_types;  ///< 需要缓存序列化的类型集合
};

/**
 * @brief 通道注册表类，管理发布/订阅关系
 * @details 该类是消息通信系统的核心组件，负责：
 *          1. 管理订阅关系和回调函数
 *          2. 注册和验证发布类型
 *          3. 提供消息路由查找功能
 *          4. 维护主题索引和缓存管理
 */
class ChannelRegistry {
 public:
  /** @brief 构造函数，初始化日志器 */
  ChannelRegistry()
      : logger_ptr_(std::make_shared<aimrt::common::util::LoggerWrapper>()) {}
  ~ChannelRegistry() = default;

  ChannelRegistry(const ChannelRegistry&) = delete;
  ChannelRegistry& operator=(const ChannelRegistry&) = delete;

  /**
   * @brief 设置日志器
   * @param logger_ptr 日志器指针
   */
  void SetLogger(const std::shared_ptr<aimrt::common::util::LoggerWrapper>& logger_ptr) { logger_ptr_ = logger_ptr; }
  
  /**
   * @brief 获取日志器
   * @return 日志器的常量引用
   */
  const aimrt::common::util::LoggerWrapper& GetLogger() const { return *logger_ptr_; }

  /**
   * @brief 注册订阅者
   * @param subscribe_wrapper_ptr 订阅包装器的唯一指针
   * @return 注册是否成功
   */
  bool Subscribe(std::unique_ptr<SubscribeWrapper>&& subscribe_wrapper_ptr);

  /**
   * @brief 注册发布类型
   * @param publish_type_wrapper_ptr 发布类型包装器的唯一指针
   * @return 注册是否成功
   */
  bool RegisterPublishType(
      std::unique_ptr<PublishTypeWrapper>&& publish_type_wrapper_ptr);

  /**
   * @brief 获取订阅包装器指针
   * @param msg_type 消息类型
   * @param topic_name 主题名称
   * @param pkg_path 包路径
   * @param module_name 模块名称
   * @return 订阅包装器指针，如果不存在返回nullptr
   */
  const SubscribeWrapper* GetSubscribeWrapperPtr(
      std::string_view msg_type,
      std::string_view topic_name,
      std::string_view pkg_path,
      std::string_view module_name) const;

  /** @brief 模块订阅包装器映射类型 */
  using ModuleSubscribeWrapperMap = std::unordered_map<std::string_view, SubscribeWrapper*>;
  
  /**
   * @brief 获取模块订阅包装器映射指针
   * @param msg_type 消息类型
   * @param topic_name 主题名称
   * @param pkg_path 包路径
   * @return 模块订阅包装器映射指针，如果不存在返回nullptr
   */
  const ModuleSubscribeWrapperMap* GetModuleSubscribeWrapperMapPtr(
      std::string_view msg_type,
      std::string_view topic_name,
      std::string_view pkg_path) const;

  /**
   * @brief 获取发布类型包装器指针
   * @param msg_type 消息类型
   * @param topic_name 主题名称
   * @param pkg_path 包路径
   * @param module_name 模块名称
   * @return 发布类型包装器指针，如果不存在返回nullptr
   */
  const PublishTypeWrapper* GetPublishTypeWrapperPtr(
      std::string_view msg_type,
      std::string_view topic_name,
      std::string_view pkg_path,
      std::string_view module_name) const;

  /** @brief 获取订阅包装器映射 */
  const auto& GetSubscribeWrapperMap() const { return subscribe_wrapper_map_; }
  /** @brief 获取发布类型包装器映射 */
  const auto& GetPublishTypeWrapperMap() const { return publish_type_wrapper_map_; }
  /** @brief 获取发布主题索引映射 */
  const auto& GetPubTopicIndexMap() const { return pub_topic_index_map_; }
  /** @brief 获取订阅主题索引映射 */
  const auto& GetSubTopicIndexMap() const { return sub_topic_index_map_; }

 private:
  /** @brief 日志器指针 */
  std::shared_ptr<aimrt::common::util::LoggerWrapper> logger_ptr_;

  /**
   * @brief 完整键结构体，用于唯一标识一个发布/订阅项
   */
  struct Key {
    std::string_view msg_type;    ///< 消息类型
    std::string_view topic_name;  ///< 主题名称
    std::string_view pkg_path;    ///< 包路径
    std::string_view module_name; ///< 模块名称

    /** @brief 相等运算符重载 */
    bool operator==(const Key& rhs) const {
      return msg_type == rhs.msg_type &&
             topic_name == rhs.topic_name &&
             pkg_path == rhs.pkg_path &&
             module_name == rhs.module_name;
    }

    /** @brief 哈希函数结构体 */
    struct Hash {
      /** @brief 哈希运算符重载 */
      std::size_t operator()(const Key& k) const {
        return (std::hash<std::string_view>()(k.msg_type)) ^
               (std::hash<std::string_view>()(k.topic_name)) ^
               (std::hash<std::string_view>()(k.pkg_path)) ^
               (std::hash<std::string_view>()(k.module_name));
      }
    };
  };

  /**
   * @brief 消息-主题-包键结构体，用于快速索引
   */
  struct MTPKey {
    std::string_view msg_type;    ///< 消息类型
    std::string_view topic_name;  ///< 主题名称
    std::string_view pkg_path;    ///< 包路径

    /** @brief 相等运算符重载 */
    bool operator==(const MTPKey& rhs) const {
      return msg_type == rhs.msg_type &&
             topic_name == rhs.topic_name &&
             pkg_path == rhs.pkg_path;
    }

    /** @brief 哈希函数结构体 */
    struct Hash {
      /** @brief 哈希运算符重载 */
      std::size_t operator()(const MTPKey& k) const {
        return (std::hash<std::string_view>()(k.msg_type)) ^
               (std::hash<std::string_view>()(k.topic_name)) ^
               (std::hash<std::string_view>()(k.pkg_path));
      }
    };
  };

  /** @brief 发布类型包装器映射，用于存储所有注册的发布类型 */
  std::unordered_map<Key, std::unique_ptr<PublishTypeWrapper>, Key::Hash>
      publish_type_wrapper_map_;

  /** @brief 发布主题索引映射，用于快速查找特定主题的所有发布类型 */
  std::unordered_map<std::string_view, std::vector<PublishTypeWrapper*>>
      pub_topic_index_map_;

  /** @brief 订阅包装器映射，用于存储所有注册的订阅者 */
  std::unordered_map<Key, std::unique_ptr<SubscribeWrapper>, Key::Hash>
      subscribe_wrapper_map_;

  /** @brief 订阅主题索引映射，用于快速查找特定主题的所有订阅者 */
  std::unordered_map<std::string_view, std::vector<SubscribeWrapper*>>
      sub_topic_index_map_;

  /** @brief 订阅消息-主题-包索引映射，用于快速查找特定模块的订阅者 */
  std::unordered_map<MTPKey, ModuleSubscribeWrapperMap, MTPKey::Hash>
      sub_msg_topic_pkg_index_map_;
};

}  // namespace aimrt::runtime::core::channel