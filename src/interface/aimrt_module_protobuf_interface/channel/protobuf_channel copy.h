#pragma once

#include <memory>
#include <functional>
#include <type_traits>
#include <thread>

#include "aimrt_module_cpp_interface/channel/channel_handle.h"
#include "aimrt_module_protobuf_interface/util/protobuf_type_support.h"

#include "aimrt_module_cpp_interface/co/inline_scheduler.h"
#include "aimrt_module_cpp_interface/co/on.h"
#include "aimrt_module_cpp_interface/co/start_detached.h"
#include "aimrt_module_cpp_interface/co/task.h"
#include "aimrt_module_cpp_interface/co/then.h"
#include "../util/type_traits.h"

namespace aimrt::channel {

template <typename MsgType, typename = std::enable_if_t<std::is_base_of<google::protobuf::Message, MsgType>::value>>
inline bool RegisterPublishType(PublisherRef publisher) {
    return publisher.RegisterPublishType(GetProtobufMessageTypeSupport<MsgType>());
}

template <typename MsgType, typename = std::enable_if_t<std::is_base_of<google::protobuf::Message, MsgType>::value>>
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

template <typename MsgType, typename = std::enable_if_t<std::is_base_of<google::protobuf::Message, MsgType>::value>>
inline void Publish(PublisherRef publisher, const MsgType& msg) {
    Publish(publisher, ContextRef(), msg);
}

template <typename MsgType, typename = std::enable_if_t<std::is_base_of<google::protobuf::Message, MsgType>::value>>
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

template <typename MsgType, typename = std::enable_if_t<std::is_base_of<google::protobuf::Message, MsgType>::value>>
inline bool SubscribeAsync(
    SubscriberRef subscriber,
    std::function<void(ContextRef, const MsgType&)>&& callback) {
    return subscriber.Subscribe(
        GetProtobufMessageTypeSupport<MsgType>(),
        [callback{std::move(callback)}](
            const aimrt_channel_context_base_t* ctx_ptr,
            const void* msg_ptr,
            aimrt_function_base_t* release_callback_base) {
            std::thread([ctx_ptr, msg_ptr, callback, release_callback_base]() {
                ContextRef ctx_ref(ctx_ptr);
                callback(ctx_ref, *(static_cast<const MsgType*>(msg_ptr)));
                SubscriberReleaseCallback(release_callback_base)();
            }).detach();
        });
}

}  // namespace aimrt::channel