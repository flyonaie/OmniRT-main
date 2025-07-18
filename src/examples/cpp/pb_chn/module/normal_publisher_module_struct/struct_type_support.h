#pragma once

#include <string>
#include <type_traits>

#include "aimrt_module_c_interface/util/type_support_base.h"
#include "aimrt_module_cpp_interface/channel/channel_handle.h"
#include "aimrt_module_cpp_interface/channel/channel_context.h"

namespace aimrt::channel {

template <typename T>
concept StructType = std::is_standard_layout_v<T> && std::is_trivial_v<T>;

template <StructType T>
inline const aimrt_type_support_base_t* GetStructTypeSupport() {
  static const aimrt_type_support_base_t type_support{
      .get_type_name = []() -> aimrt_string_view_t {
        static const std::string type_name = std::string("struct:") + typeid(T).name();
        return aimrt::util::ToAimRTStringView(type_name);
      },
      .get_type_size = []() -> size_t { return sizeof(T); },
      .serialize = [](const void* msg_ptr, void* buffer_ptr, size_t buffer_size) -> size_t {
        if (buffer_size < sizeof(T)) return 0;
        memcpy(buffer_ptr, msg_ptr, sizeof(T));
        return sizeof(T);
      },
      .deserialize = [](const void* buffer_ptr, size_t buffer_size, void* msg_ptr) -> bool {
        if (buffer_size < sizeof(T)) return false;
        memcpy(msg_ptr, buffer_ptr, sizeof(T));
        return true;
      },
      .allocate = []() -> void* { return new T(); },
      .deallocate = [](void* ptr) { delete static_cast<T*>(ptr); }};

  return &type_support;
}

template <StructType T>
inline bool RegisterPublishType(PublisherRef publisher) {
  return publisher.RegisterPublishType(GetStructTypeSupport<T>());
}

template <StructType T>
inline void Publish(PublisherRef publisher, ContextRef ctx_ref, const T& msg) {
  static const std::string kMsgTypeName = std::string("struct:") + typeid(T).name();

  if (ctx_ref) {
    if (ctx_ref.GetSerializationType().empty()) ctx_ref.SetSerializationType("struct");
    publisher.Publish(kMsgTypeName, ctx_ref, static_cast<const void*>(&msg));
    return;
  }

  Context ctx;
  ctx.SetSerializationType("struct");
  publisher.Publish(kMsgTypeName, ctx, static_cast<const void*>(&msg));
}

template <StructType T>
inline void Publish(PublisherRef publisher, const T& msg) {
  Publish(publisher, ContextRef(), msg);
}

}  // namespace aimrt::channel
