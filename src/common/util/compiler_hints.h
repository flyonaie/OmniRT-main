// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.
//
// 编译器优化提示工具
// 提供了一组宏定义，用于向编译器提供代码优化的提示信息。
// 这些提示可以帮助编译器生成更高效的机器码。
//
// 主要功能:
// 1. 分支预测提示(Branch Prediction Hints)
//    - 通过提示编译器条件分支的可能性，优化分支预测
//    - 支持C++20的[[likely]]/[[unlikely]]属性
//    - 向后兼容GCC/Clang的__builtin_expect
// 2. 函数内联提示(TODO)
//    - 控制函数是否应该被内联展开
//    - 优化函数调用开销
// 3. 内存对齐提示(TODO)
//    - 指定数据结构的内存对齐要求
//    - 提高内存访问效率
// 4. 热点/冷点函数提示(TODO)
//    - 标记频繁/罕见调用的函数
//    - 优化代码布局和缓存利用
// 5. 循环优化提示(TODO)
//    - 控制循环展开策略
//    - 优化循环执行效率
//
// 使用说明:
// - AIMRT_LIKELY(x): 提示条件x很可能为真
//   示例: if (AIMRT_LIKELY(ptr != nullptr)) { ... }
//
// - AIMRT_UNLIKELY(x): 提示条件x很可能为假
//   示例: if (AIMRT_UNLIKELY(error_occurred)) { ... }
//
// 性能影响:
// - 正确的分支预测可以显著减少CPU流水线停顿
// - 错误的预测提示可能导致性能下降
// - 建议在性能测试验证后再使用这些优化提示

#pragma once

/**
 * @brief 分支预测优化提示
 * 
 * 根据C++标准版本自动选择实现方式:
 * 1. C++20及以上: 使用标准的[[likely]]和[[unlikely]]属性
 * 2. 早期版本: 使用GCC/Clang的__builtin_expect内建函数
 * 
 * 这些提示帮助编译器:
 * - 优化机器码的分支布局
 * - 提高CPU分支预测的准确率
 * - 减少流水线停顿和性能损失
 */
#if __cplusplus >= 202002L
/// @brief 提示条件很可能为假，用于标记不太可能执行的代码路径
#define AIMRT_UNLIKELY(x) (x) [[unlikely]]
/// @brief 提示条件很可能为真，用于标记常见的代码执行路径
#define AIMRT_LIKELY(x) (x) [[likely]]
#else
/// @brief 提示条件很可能为假，用于标记不太可能执行的代码路径
#define AIMRT_UNLIKELY(x) (__builtin_expect(!!(x), 0))
/// @brief 提示条件很可能为真，用于标记常见的代码执行路径
#define AIMRT_LIKELY(x) (__builtin_expect(!!(x), 1))
#endif

// TODO: 添加更多编译器优化提示，例如:
// - 函数内联提示: ALWAYS_INLINE(强制内联), NEVER_INLINE(禁止内联)
// - 内存对齐提示: ALIGNED(n)(指定n字节对齐)
// - 热点函数提示: HOT(频繁调用), COLD(罕见调用)
// - 循环优化提示: UNROLL(展开循环), NOUNROLL(禁止展开)
