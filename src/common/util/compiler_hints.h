#pragma once

// Branch prediction hints for performance optimization
#if __cplusplus >= 202002L
#define AIMRT_UNLIKELY(x) (x) [[unlikely]]
#define AIMRT_LIKELY(x) (x) [[likely]]
#else
#define AIMRT_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#define AIMRT_LIKELY(x) (__builtin_expect(!!(x), 1))
#endif

// TODO: Add more compiler hints here, such as:
// - Function inlining hints
// - Memory alignment hints
// - Hot/cold function hints
// - Loop optimization hints
