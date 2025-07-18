#pragma once

#include <type_traits>

namespace omnirt_std {

/**
 * @brief C++17 兼容的 derived_from 概念实现
 * 
 * @details 这是一个模仿 C++20 std::derived_from 的实现，保持完全相同的语法
 */
template<typename Base>
struct derived_from {
    template<typename Derived>
    using type = std::enable_if_t<std::is_base_of_v<Base, Derived>, Derived>;
};

} // namespace omnirt_std
