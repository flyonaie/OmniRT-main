// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <chrono>
#include <optional>

#include "aimrt_module_cpp_interface/executor/executor.h"

// #include "tbb/concurrent_hash_map.h"
#include "util/atomic_hash_map.h"
#include "util/macros.h"

namespace aimrt::runtime::core::util {

/**
 * @brief RPC客户端工具类
 * 
 * @details 该类用于管理RPC客户端的消息记录和超时处理。
 * 它提供了消息记录的存储、检索以及超时回调机制。
 * 使用模板参数MsgRecorder来存储不同类型的消息记录。
 * 
 * @tparam MsgRecorder 消息记录器类型，用于存储具体的消息内容
 */
template <typename MsgRecorder>
class RpcClientTool {
 public:
  /** @brief 超时处理回调函数类型定义 */
  using TimeoutHandle = std::function<void(MsgRecorder&&)>;

 public:
  RpcClientTool() = default;
  ~RpcClientTool() = default;

  /**
   * @brief 注册超时执行器
   * 
   * @param timeout_executor 超时执行器引用
   * @throw std::runtime_error 当执行器不支持定时调度时抛出异常
   */
  void RegisterTimeoutExecutor(aimrt::executor::ExecutorRef timeout_executor) {
    if (!timeout_executor.SupportTimerSchedule())
      throw std::runtime_error("Timeout executor do not support timer schedule.");

    timeout_executor_ = timeout_executor;
  }

  /**
   * @brief 注册超时处理回调函数
   * 
   * @param timeout_handle 超时处理回调函数
   */
  void RegisterTimeoutHandle(const TimeoutHandle& timeout_handle) {
    timeout_handle_ = timeout_handle;
  }

  /**
   * @brief 记录RPC请求消息
   * 
   * @param req_id 请求ID
   * @param timeout 超时时间
   * @param msg_recorder 消息记录器
   * @return bool 记录是否成功
   * 
   * @details 将消息记录存入map中，并设置超时回调。
   * 当达到超时时间时，如果消息仍未被处理，则触发超时回调。
   */
  bool Record(uint32_t req_id, std::chrono::nanoseconds timeout, MsgRecorder&& msg_recorder) {
    client_msg_recorder_map_.Set(req_id, std::move(msg_recorder));

    if (timeout_executor_) {
      timeout_executor_.ExecuteAfter(timeout, [this, req_id]() {
        auto msg_recorder_op = GetRecord(req_id);
        if (omnirt_unlikely(msg_recorder_op))
          timeout_handle_(std::move(*msg_recorder_op));
      });
    }

    return true;
  }

  /**
   * @brief 获取指定请求ID的消息记录
   * 
   * @param req_id 请求ID
   * @return std::optional<MsgRecorder> 如果找到则返回消息记录，否则返回空
   */
  std::optional<MsgRecorder> GetRecord(uint32_t req_id) {
    MsgRecorder value;
    if (omnirt_unlikely(!client_msg_recorder_map_.Get(req_id, &value))) {
      return {};
    }
    return std::make_optional<MsgRecorder>(std::move(value));
  }

 private:
  /** @brief 超时执行器 */
  aimrt::executor::ExecutorRef timeout_executor_;
  /** @brief 超时处理回调函数 */
  TimeoutHandle timeout_handle_;

  // using ClientMsgRecorderMap = tbb::concurrent_hash_map<uint32_t, MsgRecorder>;
  /** @brief 客户端消息记录映射表，使用原子哈希表实现线程安全 */
  using ClientMsgRecorderMap = aimrt::common::util::AtomicHashMap<uint32_t, MsgRecorder>;
  ClientMsgRecorderMap client_msg_recorder_map_;
};

}  // namespace aimrt::runtime::core::util
