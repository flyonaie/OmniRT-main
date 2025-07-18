// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#ifdef CPP20
#include <concepts>
#endif // CPP20
#include <memory>

#include "aimrt_module_cpp_interface/channel/channel_handle.h"
#include "aimrt_module_protobuf_interface/util/protobuf_type_support.h"

// #include "aimrt_module_cpp_interface/co/inline_scheduler.h"
// #include "aimrt_module_cpp_interface/co/on.h"
// #include "aimrt_module_cpp_interface/co/start_detached.h"
// #include "aimrt_module_cpp_interface/co/task.h"
// #include "aimrt_module_cpp_interface/co/then.h"
#include "../util/type_traits.h"

namespace aimrt::channel {

//omnirt_std::derived_from<google::protobuf::Message> 被用作模板参数 MsgType 的约束条件，确保只有 Protocol Buffers 消息类型才能用于这些模板函数。
template <typename MsgType,
          std::enable_if_t<std::is_base_of_v<google::protobuf::Message, MsgType>, int> = 0>
inline bool RegisterPublishType(PublisherRef publisher) {
  // static constexpr bool is_protobuf_message = std::is_base_of_v<google::protobuf::Message, MsgType>;
  //   static_assert(is_protobuf_message, "MsgType必须继承自google::protobuf::Message");
  return publisher.RegisterPublishType(GetProtobufMessageTypeSupport<MsgType>());
}

template <typename MsgType,
          std::enable_if_t<std::is_base_of_v<google::protobuf::Message, MsgType>, int> = 0>
inline void Publish(PublisherRef publisher, ContextRef ctx_ref, const MsgType& msg) {
  static const std::string kMsgTypeName = "pb:" + MsgType().GetTypeName();

  if (ctx_ref) {
    if (ctx_ref.GetSerializationType().empty()) ctx_ref.SetSerializationType("pb");
    publisher.Publish(kMsgTypeName, ctx_ref, static_cast<const void*>(&msg));
    return;
  }

  Context ctx;
  ctx.SetSerializationType("pb");
  publisher.Publish(kMsgTypeName, ctx, static_cast<const void*>(&msg));
}

template <typename MsgType,
          std::enable_if_t<std::is_base_of_v<google::protobuf::Message, MsgType>, int> = 0>
inline void Publish(PublisherRef publisher, const MsgType& msg) {
  Publish(publisher, ContextRef(), msg);
}

template <typename MsgType,
          std::enable_if_t<std::is_base_of_v<google::protobuf::Message, MsgType>, int> = 0>
inline bool Subscribe(
    SubscriberRef subscriber,
    std::function<void(ContextRef, const std::shared_ptr<const MsgType>&)>&& callback) {
  return subscriber.Subscribe(
      GetProtobufMessageTypeSupport<MsgType>(),
      [callback{std::move(callback)}](
          const aimrt_channel_context_base_t* ctx_ptr,
          const void* msg_ptr,
          aimrt_function_base_t* release_callback_base) {
        SubscriberReleaseCallback release_callback(release_callback_base);
        callback(ContextRef(ctx_ptr),
                 std::shared_ptr<const MsgType>(
                     static_cast<const MsgType*>(msg_ptr),
                     [release_callback{std::move(release_callback)}](const MsgType*) { release_callback(); }));
      });
}

template <typename MsgType,
          std::enable_if_t<std::is_base_of_v<google::protobuf::Message, MsgType>, int> = 0>
inline bool Subscribe(
    SubscriberRef subscriber,
    std::function<void(const std::shared_ptr<const MsgType>&)>&& callback) {
  return subscriber.Subscribe(
      GetProtobufMessageTypeSupport<MsgType>(),
      [callback{std::move(callback)}](
          const aimrt_channel_context_base_t* ctx_ptr,
          const void* msg_ptr,
          aimrt_function_base_t* release_callback_base) {
        SubscriberReleaseCallback release_callback(release_callback_base);
        callback(std::shared_ptr<const MsgType>(
            static_cast<const MsgType*>(msg_ptr),
            [release_callback{std::move(release_callback)}](const MsgType*) { release_callback(); }));
      });
}

// #ifdef SUPPORT_COROUTINES
// template <typename MsgType,
//           std::enable_if_t<std::is_base_of_v<google::protobuf::Message, MsgType>, int>>
// inline bool SubscribeCo(
//     SubscriberRef subscriber,
//     std::function<co::Task<void>(ContextRef, const MsgType&)>&& callback) {
//   return subscriber.Subscribe(
//       GetProtobufMessageTypeSupport<MsgType>(),
//       [callback{std::move(callback)}](
//           const aimrt_channel_context_base_t* ctx_ptr,
//           const void* msg_ptr,
//           aimrt_function_base_t* release_callback_base) {
//         aimrt::co::StartDetached(
//             aimrt::co::On(
//                 aimrt::co::InlineScheduler(),
//                 callback(ContextRef(ctx_ptr), *(static_cast<const MsgType*>(msg_ptr)))) |
//             aimrt::co::Then(
//                 SubscriberReleaseCallback(release_callback_base)));
//       });
// }

// template <typename MsgType,
//           std::enable_if_t<std::is_base_of_v<google::protobuf::Message, MsgType>, int>>
// inline bool SubscribeCo(
//     SubscriberRef subscriber,
//     std::function<co::Task<void>(const MsgType&)>&& callback) {
//   return subscriber.Subscribe(
//       GetProtobufMessageTypeSupport<MsgType>(),
//       [callback{std::move(callback)}](
//           const aimrt_channel_context_base_t* ctx_ptr,
//           const void* msg_ptr,
//           aimrt_function_base_t* release_callback_base) {
//         aimrt::co::StartDetached(
//             aimrt::co::On(
//                 aimrt::co::InlineScheduler(),
//                 callback(*(static_cast<const MsgType*>(msg_ptr)))) |
//             aimrt::co::Then(
//                 SubscriberReleaseCallback(release_callback_base)));
//       });
// }
// #endif

// template<typename MsgType, 
//          typename = std::enable_if_t<std::is_base_of_v<google::protobuf::Message, MsgType>, int>>
template<typename MsgType>
class PublisherProxy : public PublisherProxyBase {
 private:
  static constexpr bool is_protobuf_message = std::is_base_of_v<google::protobuf::Message, MsgType>;
    static_assert(is_protobuf_message, "MsgType必须继承自google::protobuf::Message");
 
 public:
  explicit PublisherProxy(PublisherRef publisher)
      : PublisherProxyBase(publisher, "pb:" + MsgType().GetTypeName()) {}
  ~PublisherProxy() = default;

  static bool RegisterPublishType(PublisherRef publisher) {
    return publisher.RegisterPublishType(GetProtobufMessageTypeSupport<MsgType>());
  }

  bool RegisterPublishType() {
    return publisher_.RegisterPublishType(GetProtobufMessageTypeSupport<MsgType>());
  }

  void Publish(ContextRef ctx_ref, const MsgType& msg) {
    if (ctx_ref) {
      std::cout << "1111111ctx_ref" << std::endl;
      if (ctx_ref.GetSerializationType().empty()) ctx_ref.SetSerializationType("pb");
      PublishImpl(ctx_ref, static_cast<const void*>(&msg));
      return;
    }

    std::cout << "222222111111ctx_ref" << std::endl;
    auto ctx_ptr = NewContextSharedPtr();
    ctx_ptr->SetSerializationType("pb");
    PublishImpl(ctx_ptr, static_cast<const void*>(&msg));
  }

  void Publish(const MsgType& msg) {
    // 这里使用ContextRef(),会调用两个对象的 operator bool()
    Publish(ContextRef(), msg);
  }
};


// template<typename MsgType, 
//          typename = typename std::enable_if<std::is_base_of<google::protobuf::Message, MsgType>::value, int>::type>
template <typename MsgType>
class SubscriberProxy : public SubscriberProxyBase {
 private:
  static constexpr bool is_protobuf_message = std::is_base_of_v<google::protobuf::Message, MsgType>;
    static_assert(is_protobuf_message, "MsgType必须继承自google::protobuf::Message");

 public:
  explicit SubscriberProxy(SubscriberRef subscriber)
      : SubscriberProxyBase(subscriber, "pb:" + MsgType().GetTypeName()) {}
  ~SubscriberProxy() = default;

  bool Subscribe(
      std::function<void(ContextRef, const std::shared_ptr<const MsgType>&)>&& callback) {
    return subscriber_.Subscribe(
        GetProtobufMessageTypeSupport<MsgType>(),
        [callback{std::move(callback)}](
            const aimrt_channel_context_base_t* ctx_ptr,
            const void* msg_ptr,
            aimrt_function_base_t* release_callback_base) {
          ContextRef ctx_ref(ctx_ptr);

          SubscriberReleaseCallback release_callback(release_callback_base);
          callback(ctx_ref,
                   std::shared_ptr<const MsgType>(
                       static_cast<const MsgType*>(msg_ptr),
                       [release_callback{std::move(release_callback)}](const MsgType*) { release_callback(); }));
        });
  }

  bool Subscribe(
      std::function<void(const std::shared_ptr<const MsgType>&)>&& callback) {
    return subscriber_.Subscribe(
        GetProtobufMessageTypeSupport<MsgType>(),
        [callback{std::move(callback)}](
            const aimrt_channel_context_base_t* ctx_ptr,
            const void* msg_ptr,
            aimrt_function_base_t* release_callback_base) {
          ContextRef ctx_ref(ctx_ptr);

          SubscriberReleaseCallback release_callback(release_callback_base);
          callback(std::shared_ptr<const MsgType>(
              static_cast<const MsgType*>(msg_ptr),
              [release_callback{std::move(release_callback)}](const MsgType*) { release_callback(); }));
        });
  }

// #ifdef SUPPORT_COROUTINES
//   bool SubscribeCo(
//       std::function<co::Task<void>(ContextRef, const MsgType&)>&& callback) {
//     return subscriber_.Subscribe(
//         GetProtobufMessageTypeSupport<MsgType>(),
//         [callback{std::move(callback)}](
//             const aimrt_channel_context_base_t* ctx_ptr,
//             const void* msg_ptr,
//             aimrt_function_base_t* release_callback_base) {
//           ContextRef ctx_ref(ctx_ptr);

//           aimrt::co::StartDetached(
//               aimrt::co::On(
//                   aimrt::co::InlineScheduler(),
//                   callback(ctx_ref, *(static_cast<const MsgType*>(msg_ptr)))) |
//               aimrt::co::Then(
//                   SubscriberReleaseCallback(release_callback_base)));
//         });
//   }

//   bool SubscribeCo(std::function<co::Task<void>(const MsgType&)>&& callback) {
//     return subscriber_.Subscribe(
//         GetProtobufMessageTypeSupport<MsgType>(),
//         [callback{std::move(callback)}](
//             const aimrt_channel_context_base_t* ctx_ptr,
//             const void* msg_ptr,
//             aimrt_function_base_t* release_callback_base) {
//           ContextRef ctx_ref(ctx_ptr);

//           aimrt::co::StartDetached(
//               aimrt::co::On(
//                   aimrt::co::InlineScheduler(),
//                   callback(*(static_cast<const MsgType*>(msg_ptr)))) |
//               aimrt::co::Then(
//                   SubscriberReleaseCallback(release_callback_base)));
//         });
//   }
// #endif
};

}  // namespace aimrt::channel
