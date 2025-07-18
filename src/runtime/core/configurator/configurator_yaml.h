// yaml_auto_mapping.h
#pragma once
#include <yaml-cpp/yaml.h>

#define DEFINE_YAML_CONVERTER(Type, ...) \
namespace YAML { \
    template<> \
    struct convert<Type> { \
        static Node encode(const Type& rhs) { \
            Node node; \
            auto add_field = [&](const auto& field, const char* name) { \
                node[name] = field; \
            }; \
            EXPAND_FIELDS(rhs, add_field, __VA_ARGS__); \
            return node; \
        } \
        static bool decode(const Node& node, Type& rhs) { \
            bool all_ok = true; \
            auto get_field = [&](auto& field, const char* name) { \
                if (node[name]) field = node[name].as<std::decay_t<decltype(field)>>(); \
                else all_ok = false; \
            }; \
            EXPAND_FIELDS(rhs, get_field, __VA_ARGS__); \
            return all_ok; \
        } \
    }; \
}

// 辅助宏展开字段列表
#define EXPAND_FIELDS(obj, func, ...) \
    MACRO_MAP(func, obj, __VA_ARGS__)

// 宏处理器（支持最多10个字段）
#define MACRO_MAP(func, obj, ...) \
    MACRO_CHOOSER(__VA_ARGS__)(func, obj, __VA_ARGS__)

#define MACRO_CHOOSER(...) \
    MACRO_SELECTOR(__VA_ARGS__, MACRO_10, MACRO_9, MACRO_8, MACRO_7, MACRO_6, MACRO_5, MACRO_4, MACRO_3, MACRO_2, MACRO_1)

#define MACRO_SELECTOR(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,NAME,...) NAME

// 字段展开器
#define MACRO_1(func, obj, field) func(obj.field, #field);
#define MACRO_2(func, obj, field, ...) \
    func(obj.field, #field); \
    MACRO_1(func, obj, __VA_ARGS__)
// ... 扩展到 MACRO_10