// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "aimrt_module_c_interface/channel/channel_handle_base.h"
#include "aimrt_module_cpp_interface/util/string.h"
#include "core/channel/channel_backend_manager.h"
#include "util/log_util.h"
#include "util/string_util.h"
#include "util/macros.h"

namespace aimrt::runtime::core::channel {

/**
 * @brief 发布者代理类，负责管理消息的发布操作
 * 
 * 该类封装了消息发布的相关功能，包括类型注册、消息发布和上下文管理等
 */
class PublisherProxy {
 public:
  /**
   * @brief 构造函数
   * @param pkg_path 包路径
   * @param module_name 模块名称
   * @param topic_name 主题名称
   * @param logger 日志工具
   * @param channel_backend_manager 通道后端管理器
   * @param passed_context_meta_keys 需要传递的上下文元数据键集合
   */
  PublisherProxy(std::string_view pkg_path,
                 std::string_view module_name,
                 std::string_view topic_name,
                 aimrt::common::util::LoggerWrapper& logger,
                 ChannelBackendManager& channel_backend_manager,
                 const std::unordered_set<std::string>& passed_context_meta_keys)
      : pkg_path_(pkg_path),
        module_name_(module_name),
        topic_name_(topic_name),
        logger_(logger),
        channel_backend_manager_(channel_backend_manager),
        passed_context_meta_keys_(passed_context_meta_keys),
        base_(GenBase(this)) {}
  ~PublisherProxy() = default;

  PublisherProxy(const PublisherProxy&) = delete;
  PublisherProxy& operator=(const PublisherProxy&) = delete;

  /**
   * @brief 获取原生句柄
   * @return 返回原生发布者基础结构指针
   */
  const aimrt_channel_publisher_base_t* NativeHandle() const { return &base_; }

  /**
   * @brief 获取日志工具
   * @return 返回日志工具的常量引用
   */
  const aimrt::common::util::LoggerWrapper& GetLogger() const { return logger_; }

 private:
  /**
   * @brief 注册发布类型
   * @param msg_type_support 消息类型支持结构
   * @return 注册是否成功
   */
  bool RegisterPublishType(const aimrt_type_support_base_t* msg_type_support) {
    return channel_backend_manager_.RegisterPublishType(
        RegisterPublishTypeProxyInfoWrapper{
            .pkg_path = pkg_path_,
            .module_name = module_name_,
            .topic_name = topic_name_,
            .msg_type_support = msg_type_support});
  }

  /**
   * @brief 发布消息
   * @param msg_type 消息类型
   * @param ctx_ptr 上下文指针
   * @param msg_ptr 消息内容指针
   */
  void Publish(aimrt_string_view_t msg_type,
               const aimrt_channel_context_base_t* ctx_ptr,
               const void* msg_ptr) {
    channel_backend_manager_.Publish(
        PublishProxyInfoWrapper{
            .pkg_path = pkg_path_,
            .module_name = module_name_,
            .topic_name = topic_name_,
            .msg_type = msg_type,
            .ctx_ptr = ctx_ptr,
            .msg_ptr = msg_ptr});
  }

  /**
   * @brief 合并订阅上下文到发布上下文
   * @param subscribe_ctx_ptr 订阅上下文指针
   * @param publish_ctx_ptr 发布上下文指针
   */
  void MergeSubscribeContextToPublishContext(
      const aimrt_channel_context_base_t* subscribe_ctx_ptr, const aimrt_channel_context_base_t* publish_ctx_ptr) {
    aimrt::channel::ContextRef subscribe_ctx_ref(subscribe_ctx_ptr);
    aimrt::channel::ContextRef publish_ctx_ref(publish_ctx_ptr);

    if (omnirt_unlikely(subscribe_ctx_ref.GetType() != aimrt_channel_context_type_t::AIMRT_CHANNEL_SUBSCRIBER_CONTEXT ||
        publish_ctx_ref.GetType() != aimrt_channel_context_type_t::AIMRT_CHANNEL_PUBLISHER_CONTEXT)) {
      AIMRT_WARN("Invalid context type!");
      return;
    }

    for (const auto& key : passed_context_meta_keys_) {
      auto val = subscribe_ctx_ref.GetMetaValue(key);
      if (!val.empty()) publish_ctx_ref.SetMetaValue(key, val);
    }
  }

  /**
   * @brief 生成发布者基础结构
   * @param impl 实现指针
   * @return 返回发布者基础结构
   */
  static aimrt_channel_publisher_base_t GenBase(void* impl) {
    return aimrt_channel_publisher_base_t{
        .register_publish_type = [](void* impl, const aimrt_type_support_base_t* msg_type_support) -> bool {
          return static_cast<PublisherProxy*>(impl)->RegisterPublishType(msg_type_support);
        },
        .publish = [](void* impl,
                      aimrt_string_view_t msg_type,
                      const aimrt_channel_context_base_t* ctx_ptr,
                      const void* msg_ptr) {
          static_cast<PublisherProxy*>(impl)->Publish(msg_type, ctx_ptr, msg_ptr);  //
        },
        .get_topic = [](void* impl) -> aimrt_string_view_t {
          return aimrt::util::ToAimRTStringView(static_cast<PublisherProxy*>(impl)->topic_name_);
        },
        .merge_subscribe_context_to_publish_context = [](void* impl,                                             //
                                                         const aimrt_channel_context_base_t* subscribe_ctx_ptr,  //
                                                         const aimrt_channel_context_base_t* publish_ctx_ptr) {  //
          static_cast<PublisherProxy*>(impl)->MergeSubscribeContextToPublishContext(subscribe_ctx_ptr, publish_ctx_ptr);
        },
        .impl = impl};
  }

 private:
  const std::string pkg_path_;
  const std::string module_name_;
  const std::string topic_name_;

  aimrt::common::util::LoggerWrapper& logger_;

  ChannelBackendManager& channel_backend_manager_;

  const std::unordered_set<std::string>& passed_context_meta_keys_;

  const aimrt_channel_publisher_base_t base_;
};

/**
 * @brief 订阅者代理类，负责管理消息的订阅操作
 * 
 * 该类封装了消息订阅的相关功能，包括订阅注册和回调处理等
 */
class SubscriberProxy {
 public:
  /**
   * @brief 构造函数
   * @param pkg_path 包路径
   * @param module_name 模块名称
   * @param topic_name 主题名称
   * @param channel_backend_manager 通道后端管理器
   */
  SubscriberProxy(std::string_view pkg_path,
                  std::string_view module_name,
                  std::string_view topic_name,
                  ChannelBackendManager& channel_backend_manager)
      : pkg_path_(pkg_path),
        module_name_(module_name),
        topic_name_(topic_name),
        channel_backend_manager_(channel_backend_manager),
        base_(GenBase(this)) {}
  ~SubscriberProxy() = default;

  SubscriberProxy(const SubscriberProxy&) = delete;
  SubscriberProxy& operator=(const SubscriberProxy&) = delete;

  /**
   * @brief 获取原生句柄
   * @return 返回原生订阅者基础结构指针
   */
  const aimrt_channel_subscriber_base_t* NativeHandle() const { return &base_; }

 private:
  /**
   * @brief 订阅消息
   * @param msg_type_support 消息类型支持结构
   * @param callback 回调函数
   * @return 订阅是否成功
   */
  bool Subscribe(const aimrt_type_support_base_t* msg_type_support,
                 aimrt_function_base_t* callback) {
    return channel_backend_manager_.Subscribe(
        SubscribeProxyInfoWrapper{
            .pkg_path = pkg_path_,
            .module_name = module_name_,
            .topic_name = topic_name_,
            .msg_type_support = msg_type_support,
            .callback = callback});
  }

  /**
   * @brief 生成订阅者基础结构
   * @param impl 实现指针
   * @return 返回订阅者基础结构
   */
  static aimrt_channel_subscriber_base_t GenBase(void* impl) {
    return aimrt_channel_subscriber_base_t{
        .subscribe = [](void* impl,
                        const aimrt_type_support_base_t* msg_type_support,
                        aimrt_function_base_t* callback) -> bool {
          return static_cast<SubscriberProxy*>(impl)->Subscribe(msg_type_support, callback);
        },
        .get_topic = [](void* impl) -> aimrt_string_view_t {
          return aimrt::util::ToAimRTStringView(static_cast<SubscriberProxy*>(impl)->topic_name_);
        },
        .impl = impl};
  }

 private:
  const std::string pkg_path_;
  const std::string module_name_;
  const std::string topic_name_;

  ChannelBackendManager& channel_backend_manager_;

  const aimrt_channel_subscriber_base_t base_;
};

/**
 * @brief 通道句柄代理类，统一管理发布者和订阅者
 * 
 * 该类作为发布订阅系统的主要接口，负责创建和管理发布者与订阅者代理对象
 */
class ChannelHandleProxy {
 public:
  /**
   * @brief 发布者代理映射表类型定义
   */
  using PublisherProxyMap = std::unordered_map<
      std::string,
      std::unique_ptr<PublisherProxy>,
      aimrt::common::util::StringHash>;  // Use StringHash for heterogeneous lookup

  /**
   * @brief 订阅者代理映射表类型定义
   */
  using SubscriberProxyMap = std::unordered_map<
      std::string,
      std::unique_ptr<SubscriberProxy>,
      aimrt::common::util::StringHash>;  // Use StringHash for heterogeneous lookup


  /**
   * @brief 构造函数
   * @param pkg_path 包路径
   * @param module_name 模块名称
   * @param logger 日志工具
   * @param channel_backend_manager 通道后端管理器
   * @param passed_context_meta_keys 需要传递的上下文元数据键集合
   * @param start_flag 启动标志
   * @param publisher_proxy_map 发布者代理映射表
   * @param subscriber_proxy_map 订阅者代理映射表
   */
  ChannelHandleProxy(std::string_view pkg_path,
                     std::string_view module_name,
                     aimrt::common::util::LoggerWrapper& logger,
                     ChannelBackendManager& channel_backend_manager,
                     const std::unordered_set<std::string>& passed_context_meta_keys,
                     std::atomic_bool& start_flag,
                     PublisherProxyMap& publisher_proxy_map,
                     SubscriberProxyMap& subscriber_proxy_map)
      : pkg_path_(pkg_path),
        module_name_(module_name),
        logger_(logger),
        channel_backend_manager_(channel_backend_manager),
        passed_context_meta_keys_(passed_context_meta_keys),
        start_flag_(start_flag),
        publisher_proxy_map_(publisher_proxy_map),
        subscriber_proxy_map_(subscriber_proxy_map),
        base_(GenBase(this)) {}

  ~ChannelHandleProxy() = default;

  ChannelHandleProxy(const ChannelHandleProxy&) = delete;
  ChannelHandleProxy& operator=(const ChannelHandleProxy&) = delete;

  /**
   * @brief 获取原生句柄
   * @return 返回原生通道句柄基础结构指针
   */
  const aimrt_channel_handle_base_t* NativeHandle() const { return &base_; }

  /**
   * @brief 获取日志工具
   * @return 返回日志工具的常量引用
   */
  const aimrt::common::util::LoggerWrapper& GetLogger() const { return logger_; }

 private:
  /**
   * @brief 获取指定主题的发布者
   * @param input_topic 主题名称
   * @return 返回发布者基础结构指针，如果获取失败则返回nullptr
   */
  const aimrt_channel_publisher_base_t* GetPublisher(aimrt_string_view_t input_topic) {
    auto topic = aimrt::util::ToStdStringView(input_topic);
    auto find_itr = publisher_proxy_map_.find(std::string(topic));
    if (find_itr != publisher_proxy_map_.end()) {
      return find_itr->second->NativeHandle();
    }

    if (start_flag_.load()) {
      AIMRT_WARN("Can not get new publisher for topic '{}' after start.", topic);
      return nullptr;
    }

    AIMRT_TRACE("Create new publisher for topic '{}'.", topic);
    auto emplace_ret = publisher_proxy_map_.emplace(
        std::string(topic),
        std::make_unique<PublisherProxy>(
            pkg_path_,
            module_name_,
            topic,
            logger_,
            channel_backend_manager_,
            passed_context_meta_keys_));

    return emplace_ret.first->second->NativeHandle();
  }

  /**
   * @brief 获取指定主题的订阅者
   * @param input_topic 主题名称
   * @return 返回订阅者基础结构指针，如果获取失败则返回nullptr
   */
  const aimrt_channel_subscriber_base_t* GetSubscriber(aimrt_string_view_t input_topic) {
    auto topic = aimrt::util::ToStdStringView(input_topic);
    auto find_itr = subscriber_proxy_map_.find(std::string(topic));
    if (find_itr != subscriber_proxy_map_.end()) {
      return find_itr->second->NativeHandle();
    }

    if (start_flag_.load()) {
      AIMRT_WARN("Can not get new subscriber for topic '{}' after start.", topic);
      return nullptr;
    }

    auto emplace_ret = subscriber_proxy_map_.emplace(
        std::string(topic),
        std::make_unique<SubscriberProxy>(
            pkg_path_,
            module_name_,
            topic,
            channel_backend_manager_));

    return emplace_ret.first->second->NativeHandle();
  }

  /**
   * @brief 合并订阅上下文到发布上下文
   * @param subscribe_ctx_ptr 订阅上下文指针
   * @param publish_ctx_ptr 发布上下文指针
   */
  void MergeSubscribeContextToPublishContext(
      const aimrt_channel_context_base_t* subscribe_ctx_ptr, const aimrt_channel_context_base_t* publish_ctx_ptr) {
    aimrt::channel::ContextRef subscribe_ctx_ref(subscribe_ctx_ptr);
    aimrt::channel::ContextRef publish_ctx_ref(publish_ctx_ptr);

    if (omnirt_unlikely(subscribe_ctx_ref.GetType() != aimrt_channel_context_type_t::AIMRT_CHANNEL_SUBSCRIBER_CONTEXT ||
        publish_ctx_ref.GetType() != aimrt_channel_context_type_t::AIMRT_CHANNEL_PUBLISHER_CONTEXT)) {
      AIMRT_WARN("Invalid context type!");
      return;
    }

    for (const auto& key : passed_context_meta_keys_) {
      auto val = subscribe_ctx_ref.GetMetaValue(key);
      if (!val.empty()) publish_ctx_ref.SetMetaValue(key, val);
    }
  }

  /**
   * @brief 生成通道句柄基础结构
   * @param impl 实现指针
   * @return 返回通道句柄基础结构
   */
  static aimrt_channel_handle_base_t GenBase(void* impl) {
    return aimrt_channel_handle_base_t{
        .get_publisher = [](void* impl, aimrt_string_view_t topic) -> const aimrt_channel_publisher_base_t* {
          return static_cast<ChannelHandleProxy*>(impl)->GetPublisher(topic);
        },
        .get_subscriber = [](void* impl, aimrt_string_view_t topic) -> const aimrt_channel_subscriber_base_t* {
          return static_cast<ChannelHandleProxy*>(impl)->GetSubscriber(topic);
        },
        .merge_subscribe_context_to_publish_context = [](void* impl,                                             //
                                                         const aimrt_channel_context_base_t* subscribe_ctx_ptr,  //
                                                         const aimrt_channel_context_base_t* publish_ctx_ptr) {  //
          static_cast<ChannelHandleProxy*>(impl)->MergeSubscribeContextToPublishContext(subscribe_ctx_ptr, publish_ctx_ptr);
        },
        .impl = impl};
  }

 private:
  const std::string pkg_path_;
  const std::string module_name_;

  aimrt::common::util::LoggerWrapper& logger_;

  ChannelBackendManager& channel_backend_manager_;

  const std::unordered_set<std::string>& passed_context_meta_keys_;

  std::atomic_bool& start_flag_;
  PublisherProxyMap& publisher_proxy_map_;
  SubscriberProxyMap& subscriber_proxy_map_;

  const aimrt_channel_handle_base_t base_;
};

}  // namespace aimrt::runtime::core::channel
