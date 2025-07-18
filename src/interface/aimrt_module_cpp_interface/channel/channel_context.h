// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <string_view>
#include <unordered_map>

#include "aimrt_module_c_interface/channel/channel_context_base.h"
#include "aimrt_module_cpp_interface/util/string.h"
#include "util/exception.h"
#include "util/string_util.h"

namespace aimrt::channel {

/**
 * @brief 通道上下文类，用于管理通道通信过程中的上下文信息
 * @details 该类负责存储和管理通道（Channel）通信过程中的元数据信息，
 *          包括序列化类型、使用状态等。支持发布者和订阅者两种上下文类型。
 */
class Context {
 public:
  /**
   * @brief 构造函数
   * @param type 上下文类型，默认为发布者上下文
   */
  Context(aimrt_channel_context_type_t type = aimrt_channel_context_type_t::AIMRT_CHANNEL_PUBLISHER_CONTEXT)
      : type_(type),
        base_(aimrt_channel_context_base_t{
            .ops = GenOpsBase(),
            .impl = this}) {}
  ~Context() = default;

  /**
   * @brief 获取原生句柄
   * @return 返回底层C接口的上下文结构指针
   */
  const aimrt_channel_context_base_t* NativeHandle() const { return &base_; }

  /**
   * @brief 检查上下文是否已被使用
   * @return true表示已使用，false表示未使用
   */
  bool CheckUsed() const { return used_; }
  
  /**
   * @brief 标记上下文为已使用状态
   */
  void SetUsed() { used_ = true; }

  /**
   * @brief 重置上下文状态
   * @details 清空所有元数据并将使用状态重置为未使用
   */
  void Reset() {
    used_ = false;
    meta_data_map_.clear();
    meta_keys_vec_.clear();
  }

  /**
   * @brief 获取上下文类型
   * @return 返回上下文类型（发布者或订阅者）
   */
  aimrt_channel_context_type_t GetType() const { return type_; }

  /**
   * @brief 获取指定键的元数据值
   * @param key 元数据键
   * @return 返回对应的值，如果键不存在则返回空字符串
   */
  std::string_view GetMetaValue(std::string_view key) const {
    auto finditr = meta_data_map_.find(std::string(key));
    if (finditr != meta_data_map_.end()) return finditr->second;
    return "";
  }

  /**
   * @brief 设置元数据键值对
   * @param key 元数据键
   * @param val 元数据值
   */
  void SetMetaValue(std::string_view key, std::string_view val) {
    auto finditr = meta_data_map_.find(std::string(key));
    if (finditr == meta_data_map_.end()) {
      meta_data_map_.emplace(std::string(key), std::string(val));
    } else {
      finditr->second = std::string(val);
    }
  }

  /**
   * @brief 获取所有元数据键
   * @return 返回所有元数据键的列表
   */
  std::vector<std::string_view> GetMetaKeys() const {
    std::vector<std::string_view> result;
    result.reserve(meta_data_map_.size());
    for (const auto& it : meta_data_map_) result.emplace_back(it.first);
    return result;
  }

  /**
   * @brief 获取序列化类型
   * @return 返回当前使用的序列化类型
   */
  std::string_view GetSerializationType() const {
    return GetMetaValue(AIMRT_CHANNEL_CONTEXT_KEY_SERIALIZATION_TYPE);
  }

  /**
   * @brief 设置序列化类型
   * @param val 序列化类型值
   */
  void SetSerializationType(std::string_view val) {
    SetMetaValue(AIMRT_CHANNEL_CONTEXT_KEY_SERIALIZATION_TYPE, val);
  }

  /**
   * @brief 将上下文信息转换为字符串
   * @return 返回包含上下文类型和所有元数据的JSON格式字符串
   */
  std::string ToString() const {
    std::stringstream ss;
    if (type_ == aimrt_channel_context_type_t::AIMRT_CHANNEL_PUBLISHER_CONTEXT) {
      ss << "Publisher context, ";
    } else if (type_ == aimrt_channel_context_type_t::AIMRT_CHANNEL_SUBSCRIBER_CONTEXT) {
      ss << "Subscriber context, ";
    } else {
      ss << "Unknown context, ";
    }

    ss << "meta: {";

    for (auto itr = meta_data_map_.begin(); itr != meta_data_map_.end(); ++itr) {
      if (itr != meta_data_map_.begin()) ss << ",";
      ss << "{\"" << itr->first << "\":\"" << itr->second << "\"}";
    }

    ss << "}";

    return ss.str();
  }

 private:
  /**
   * @brief 生成基础操作函数表
   * @return 返回C接口操作函数表指针
   */
  static const aimrt_channel_context_base_ops_t* GenOpsBase() {
    static constexpr aimrt_channel_context_base_ops_t kOps{
        .check_used = [](void* impl) -> bool {
          return static_cast<Context*>(impl)->used_;
        },
        .set_used = [](void* impl) {
          static_cast<Context*>(impl)->used_ = true;
        },
        .get_type = [](void* impl) -> aimrt_channel_context_type_t {
          return static_cast<Context*>(impl)->type_;
        },
        .get_meta_val = [](void* impl, aimrt_string_view_t key) -> aimrt_string_view_t {
          return aimrt::util::ToAimRTStringView(
              static_cast<Context*>(impl)->GetMetaValue(aimrt::util::ToStdStringView(key)));
        },
        .set_meta_val = [](void* impl, aimrt_string_view_t key, aimrt_string_view_t val) {
          static_cast<Context*>(impl)->SetMetaValue(
              aimrt::util::ToStdStringView(key), aimrt::util::ToStdStringView(val));
        },
        .get_meta_keys = [](void* impl) -> aimrt_string_view_array_t {
          const auto& meta_data_map = static_cast<Context*>(impl)->meta_data_map_;
          auto& meta_keys_vec = static_cast<Context*>(impl)->meta_keys_vec_;

          meta_keys_vec.clear();
          meta_keys_vec.reserve(meta_data_map.size());

          for (const auto& it : meta_data_map)
            meta_keys_vec.emplace_back(aimrt::util::ToAimRTStringView(it.first));

          return aimrt_string_view_array_t{
              .str_array = meta_keys_vec.data(),
              .len = meta_keys_vec.size()};
        }};

    return &kOps;
  }

 private:
  bool used_ = false;  ///< 上下文使用状态标志
  
  /** @brief 元数据存储映射表 */
  std::unordered_map<
      std::string,
      std::string,
      aimrt::common::util::StringHash,
      std::equal_to<>>
      meta_data_map_;

  std::vector<aimrt_string_view_t> meta_keys_vec_;  ///< 元数据键列表缓存
  const aimrt_channel_context_type_t type_;  ///< 上下文类型（发布者/订阅者）
  const aimrt_channel_context_base_t base_;  ///< C接口基础结构
};

/**
 * @brief 通道上下文引用类，提供对Context对象的轻量级引用访问
 * @details 该类作为Context类的引用包装器，提供了一种安全的方式来访问和操作Context对象。
 *          它通过指针维护对原始Context对象的引用，并提供了一系列接口来访问Context的功能。
 */
class ContextRef {
 public:
  // = default 告诉编译器使用其默认实现,默认构造函数会将类中的成员变量 base_ptr_ 初始化为 nullptr
  ContextRef() = default;

  /**
   * @brief 从Context对象构造引用
   * @param ctx Context对象
   */
  ContextRef(const Context& ctx)
      : base_ptr_(ctx.NativeHandle()) {}

  /**
   * @brief 从Context指针构造引用
   * @param ctx_ptr Context对象指针
   */
  ContextRef(const Context* ctx_ptr)
      : base_ptr_(ctx_ptr ? ctx_ptr->NativeHandle() : nullptr) {}

  /**
   * @brief 从Context智能指针构造引用
   * @param ctx_ptr Context对象的智能指针
   */
  ContextRef(const std::shared_ptr<Context>& ctx_ptr)
      : base_ptr_(ctx_ptr ? ctx_ptr->NativeHandle() : nullptr) {}

  /**
   * @brief 从原生句柄构造引用
   * @param base_ptr C接口上下文结构指针
   */
  explicit ContextRef(const aimrt_channel_context_base_t* base_ptr)
      : base_ptr_(base_ptr) {}

  ~ContextRef() = default;

  /**
   * @brief 布尔操作符重载，用于检查引用是否有效
   * @return 如果引用有效返回true，否则返回false
   */
  explicit operator bool() const { return (base_ptr_ != nullptr); }

  /**
   * @brief 获取原生句柄
   * @return 返回C接口上下文结构指针
   */
  const aimrt_channel_context_base_t* NativeHandle() const {
    return base_ptr_;
  }

  /**
   * @brief 检查上下文是否已被使用
   * @return true表示已使用，false表示未使用
   * @throw 如果引用无效则抛出异常
   */
  bool CheckUsed() const {
    AIMRT_ASSERT(base_ptr_ && base_ptr_->ops, "Reference is null.");
    return base_ptr_->ops->check_used(base_ptr_->impl);
  }

  /**
   * @brief 标记上下文为已使用状态
   * @throw 如果引用无效则抛出异常
   */
  void SetUsed() {
    AIMRT_ASSERT(base_ptr_ && base_ptr_->ops, "Reference is null.");
    base_ptr_->ops->set_used(base_ptr_->impl);
  }

  /**
   * @brief 获取上下文类型
   * @return 返回上下文类型（发布者或订阅者）
   * @throw 如果引用无效则抛出异常
   */
  aimrt_channel_context_type_t GetType() const {
    AIMRT_ASSERT(base_ptr_ && base_ptr_->ops, "Reference is null.");
    return base_ptr_->ops->get_type(base_ptr_->impl);
  }

  /**
   * @brief 获取指定键的元数据值
   * @param key 元数据键
   * @return 返回对应的值
   * @throw 如果引用无效则抛出异常
   */
  std::string_view GetMetaValue(std::string_view key) const {
    AIMRT_ASSERT(base_ptr_ && base_ptr_->ops, "Reference is null.");
    return aimrt::util::ToStdStringView(
        base_ptr_->ops->get_meta_val(base_ptr_->impl, aimrt::util::ToAimRTStringView(key)));
  }

  /**
   * @brief 设置元数据键值对
   * @param key 元数据键
   * @param val 元数据值
   * @throw 如果引用无效则抛出异常
   */
  void SetMetaValue(std::string_view key, std::string_view val) {
    AIMRT_ASSERT(base_ptr_ && base_ptr_->ops, "Reference is null.");
    base_ptr_->ops->set_meta_val(
        base_ptr_->impl, aimrt::util::ToAimRTStringView(key), aimrt::util::ToAimRTStringView(val));
  }

  /**
   * @brief 获取所有元数据键
   * @return 返回所有元数据键的列表
   * @throw 如果引用无效则抛出异常
   */
  std::vector<std::string_view> GetMetaKeys() const {
    AIMRT_ASSERT(base_ptr_ && base_ptr_->ops, "Reference is null.");
    aimrt_string_view_array_t keys = base_ptr_->ops->get_meta_keys(base_ptr_->impl);

    std::vector<std::string_view> result;
    result.reserve(keys.len);
    for (size_t ii = 0; ii < keys.len; ++ii)
      result.emplace_back(aimrt::util::ToStdStringView(keys.str_array[ii]));

    return result;
  }

  /**
   * @brief 获取序列化类型
   * @return 返回当前使用的序列化类型
   */
  std::string_view GetSerializationType() const {
    return GetMetaValue(AIMRT_CHANNEL_CONTEXT_KEY_SERIALIZATION_TYPE);
  }

  /**
   * @brief 设置序列化类型
   * @param val 序列化类型值
   */
  void SetSerializationType(std::string_view val) {
    SetMetaValue(AIMRT_CHANNEL_CONTEXT_KEY_SERIALIZATION_TYPE, val);
  }

  /**
   * @brief 将上下文信息转换为字符串
   * @return 返回包含上下文类型和所有元数据的JSON格式字符串
   * @throw 如果引用无效则抛出异常
   */
  std::string ToString() const {
    AIMRT_ASSERT(base_ptr_ && base_ptr_->ops, "Reference is null.");

    std::stringstream ss;

    auto type = base_ptr_->ops->get_type(base_ptr_->impl);
    if (type == aimrt_channel_context_type_t::AIMRT_CHANNEL_PUBLISHER_CONTEXT) {
      ss << "Publisher context, ";
    } else if (type == aimrt_channel_context_type_t::AIMRT_CHANNEL_SUBSCRIBER_CONTEXT) {
      ss << "Subscriber context, ";
    } else {
      ss << "Unknown context, ";
    }

    ss << "meta: {";

    aimrt_string_view_array_t keys = base_ptr_->ops->get_meta_keys(base_ptr_->impl);
    for (size_t ii = 0; ii < keys.len; ++ii) {
      if (ii != 0) ss << ",";
      auto val = base_ptr_->ops->get_meta_val(base_ptr_->impl, keys.str_array[ii]);
      ss << "{\"" << aimrt::util::ToStdStringView(keys.str_array[ii])
         << "\":\"" << aimrt::util::ToStdStringView(val) << "\"}";
    }

    ss << "}";

    return ss.str();
  }

 private:
  const aimrt_channel_context_base_t* base_ptr_;  ///< C接口上下文结构指针
};

}  // namespace aimrt::channel