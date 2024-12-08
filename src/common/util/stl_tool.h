// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace aimrt::common::util {

// 定义可迭代类型的SFINAE检查
// 要求类型T必须：
// 1. 有 begin() 方法返回迭代器
// 2. 有 end() 方法返回迭代器
// 3. 有 size() 方法返回大小
template <typename T, typename = void>
struct is_iterable : std::false_type {};

template <typename T>
struct is_iterable<T,
    std::void_t<
        typename T::iterator,
        typename T::value_type,
        typename std::enable_if_t<
            std::is_same_v<
                decltype(std::declval<T>().begin()),
                typename T::iterator>>,
        typename std::enable_if_t<
            std::is_same_v<
                decltype(std::declval<T>().end()),
                typename T::iterator>>,
        typename std::enable_if_t<
            std::is_same_v<
                decltype(std::declval<T>().size()),
                typename T::size_type>>
    >> : std::true_type {};

template <typename T>
inline constexpr bool is_iterable_v = is_iterable<T>::value;

// 定义映射类型的SFINAE检查
// 要求类型T必须：
// 1. 满足可迭代类型的所有要求
// 2. 支持使用[]操作符访问元素
template <typename T, typename = void>
struct is_map : std::false_type {};

template <typename T>
struct is_map<T,
    std::void_t<
        typename T::key_type,
        typename T::mapped_type,
        typename std::enable_if_t<is_iterable_v<T>>,
        decltype(std::declval<T>()[std::declval<typename T::key_type>()])
    >> : std::true_type {};

template <typename T>
inline constexpr bool is_map_v = is_map<T>::value;

// 将容器转换为字符串表示
// @param t: 输入容器
// @param f: 自定义的元素转换函数
// @return: 格式化后的字符串
template <typename T, typename = std::enable_if_t<is_iterable_v<T>>>
std::string Container2Str(
    const T& t, const std::function<std::string(const typename T::value_type&)>& f) {
  std::stringstream ss;
  ss << "size = " << t.size() << '\n';
  if (!f) return ss.str();

  constexpr size_t kMaxLineLen = 32;  // 每行最大长度

  size_t ct = 0;
  for (const auto& itr : t) {
    std::string obj_str = f(itr);
    if (obj_str.empty()) obj_str = "<empty string>";

    ss << "[index=" << ct << "]:";
    // 如果字符串太长或包含换行符，先换行再输出内容
    if (obj_str.length() > kMaxLineLen || obj_str.find('\n') != std::string::npos) {
      ss << '\n';
    }

    ss << obj_str << '\n';

    ++ct;
  }
  return ss.str();
}

// Container2Str的简化版本，使用默认的流输出操作符
template <typename T, typename = std::enable_if_t<is_iterable_v<T>>>
std::string Container2Str(const T& t) {
  return Container2Str(
      t,
      [](const typename T::value_type& obj) {
        std::stringstream ss;
        ss << obj;
        return ss.str();
      });
}

// 将映射类型转换为字符串表示
// @param m: 输入映射
// @param fkey: 键转换函数
// @param fval: 值转换函数
// @return: 格式化后的字符串
template <typename T, typename = std::enable_if_t<is_map_v<T>>>
std::string Map2Str(const T& m,
                    const std::function<std::string(const typename T::key_type&)>& fkey,
                    const std::function<std::string(const typename T::mapped_type&)>& fval) {
  std::stringstream ss;
  ss << "size = " << m.size() << '\n';
  if (!fkey) return ss.str();

  constexpr size_t kMaxLineLen = 32;

  size_t ct = 0;
  for (const auto& itr : m) {
    std::string key_str = fkey(itr.first);
    if (key_str.empty()) key_str = "<empty string>";

    std::string val_str;
    if (fval) {
      val_str = fval(itr.second);
      if (val_str.empty()) val_str = "<empty string>";
    } else {
      val_str = "<unable to print>";
    }

    ss << "[index=" << ct << "]:\n  [key]:";
    if (key_str.length() > kMaxLineLen || key_str.find('\n') != std::string::npos) {
      ss << '\n';
    }

    ss << key_str << "\n  [val]:";
    if (val_str.length() > kMaxLineLen || val_str.find('\n') != std::string::npos) {
      ss << '\n';
    }

    ss << val_str << '\n';

    ++ct;
  }
  return ss.str();
}

// Map2Str的简化版本，使用默认的流输出操作符
template <typename T, typename = std::enable_if_t<is_map_v<T>>>
std::string Map2Str(const T& m) {
  return Map2Str(
      m,
      [](const typename T::key_type& obj) {
        std::stringstream ss;
        ss << obj;
        return ss.str();
      },
      [](const typename T::mapped_type& obj) {
        std::stringstream ss;
        ss << obj;
        return ss.str();
      });
}

// 检查两个容器是否相等（考虑顺序）
// @param t1, t2: 要比较的两个容器
// @return: 如果容器大小相等且对应位置的元素都相等，返回true
template <typename T, typename = std::enable_if_t<is_iterable_v<T>>>
bool CheckContainerEqual(const T& t1, const T& t2) {
  if (t1.size() != t2.size()) return false;

  auto len = t1.size();
  auto itr1 = t1.begin();
  auto itr2 = t2.begin();

  for (size_t ii = 0; ii < len; ++ii) {
    if (*itr1 != *itr2) return false;
    ++itr1;
    ++itr2;
  }
  return true;
}

// 检查两个容器是否相等（不考虑顺序）
// @param input_t1, input_t2: 要比较的两个容器
// @return: 如果容器大小相等且包含相同的元素（忽略顺序），返回true
template <typename T, typename = std::enable_if_t<is_iterable_v<T>>>
bool CheckContainerEqualNoOrder(const T& input_t1, const T& input_t2) {
  auto t1(input_t1);
  auto t2(input_t2);

  // 先排序再比较
  std::sort(t1.begin(), t1.end());
  std::sort(t2.begin(), t2.end());

  if (t1.size() != t2.size()) return false;

  auto len = t1.size();
  auto itr1 = t1.begin();
  auto itr2 = t2.begin();

  for (size_t ii = 0; ii < len; ++ii) {
    if (*itr1 != *itr2) return false;
    ++itr1;
    ++itr2;
  }
  return true;
}

// 检查两个映射是否相等
// @param map1, map2: 要比较的两个映射
// @return: 如果映射大小相等且所有键值对都相等，返回true
template <typename T, typename = std::enable_if_t<is_map_v<T>>>
bool CheckMapEqual(const T& map1, const T& map2) {
  if (map1.size() != map2.size()) return false;

  auto len = map1.size();
  auto itr1 = map1.begin();
  auto itr2 = map2.begin();

  for (size_t ii = 0; ii < len; ++ii) {
    if ((itr1->first != itr2->first) || (itr1->second != itr2->second))
      return false;
    ++itr1;
    ++itr2;
  }
  return true;
}

}  // namespace aimrt::common::util
