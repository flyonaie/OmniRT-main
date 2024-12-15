// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

/**
 * @file stl_tool.h
 * @brief STL工具库，提供容器操作和类型特征检查的通用工具
 * 
 * 本文件包含以下主要功能：
 * 1. 类型特征检查：判断类型是否为可迭代容器或映射类型
 * 2. 容器转字符串：将STL容器转换为可读的字符串格式
 * 3. 容器比较：提供带序和无序的容器比较功能
 * 
 * 设计特点：
 * - 使用SFINAE和类型特征实现编译期类型检查
 * - 支持自定义格式化函数
 * - 提供直观的调试输出格式
 * 
 * @note 所有函数都是线程安全的，因为它们不修改输入参数
 */

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

/**
 * @brief 检查类型是否为可迭代容器的类型特征
 * 
 * 一个类型要被认为是可迭代的，必须满足以下条件：
 * 1. 定义了iterator类型
 * 2. 定义了value_type类型
 * 3. 提供begin()方法返回iterator
 * 4. 提供end()方法返回iterator
 * 5. 提供size()方法返回size_type
 * 
 * @tparam T 要检查的类型
 * @tparam void SFINAE辅助参数
 */
template <typename T, typename = void>
struct is_iterable : std::false_type {};

/**
 * @brief is_iterable的特化版本，用于检查类型是否满足可迭代要求
 * 
 * 使用std::void_t和SFINAE技术检查类型是否具有必要的成员和方法
 */
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

/// @brief is_iterable的辅助变量模板
template <typename T>
inline constexpr bool is_iterable_v = is_iterable<T>::value;

/**
 * @brief 检查类型是否为映射容器的类型特征
 * 
 * 一个类型要被认为是映射容器，必须满足以下条件：
 * 1. 满足可迭代容器的所有要求
 * 2. 定义了key_type类型
 * 3. 定义了mapped_type类型
 * 4. 支持operator[]访问元素
 * 
 * @tparam T 要检查的类型
 * @tparam void SFINAE辅助参数
 */
template <typename T, typename = void>
struct is_map : std::false_type {};

/**
 * @brief is_map的特化版本，用于检查类型是否满足映射容器要求
 */
template <typename T>
struct is_map<T,
    std::void_t<
        typename T::key_type,
        typename T::mapped_type,
        typename std::enable_if_t<is_iterable_v<T>>,
        decltype(std::declval<T>()[std::declval<typename T::key_type>()])
    >> : std::true_type {};

/// @brief is_map的辅助变量模板
template <typename T>
inline constexpr bool is_map_v = is_map<T>::value;

/**
 * @brief 将容器转换为格式化的字符串表示
 * 
 * 提供详细的容器内容展示，包括：
 * - 容器大小
 * - 每个元素的索引
 * - 元素的自定义格式化输出
 * 
 * 输出格式示例：
 * size = 3
 * [index=0]: value1
 * [index=1]: value2
 * [index=2]: value3
 * 
 * @tparam T 容器类型，必须满足is_iterable要求
 * @param t 要转换的容器
 * @param f 元素转换函数，接收value_type返回string
 * @return 格式化后的字符串
 */
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

/**
 * @brief Container2Str的简化版本，使用operator<<进行元素转换
 * 
 * @tparam T 容器类型，必须满足is_iterable要求
 * @param t 要转换的容器
 * @return 格式化后的字符串
 */
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

/**
 * @brief 将映射容器转换为格式化的字符串表示
 * 
 * 提供详细的映射内容展示，包括：
 * - 映射大小
 * - 每个键值对的索引
 * - 键和值的自定义格式化输出
 * 
 * 输出格式示例：
 * size = 2
 * [index=0]:
 *   [key]: key1
 *   [val]: value1
 * [index=1]:
 *   [key]: key2
 *   [val]: value2
 * 
 * @tparam T 映射类型，必须满足is_map要求
 * @param m 要转换的映射
 * @param fkey 键转换函数
 * @param fval 值转换函数
 * @return 格式化后的字符串
 */
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

/**
 * @brief Map2Str的简化版本，使用operator<<进行键值转换
 * 
 * @tparam T 映射类型，必须满足is_map要求
 * @param m 要转换的映射
 * @return 格式化后的字符串
 */
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

/**
 * @brief 检查两个容器是否相等（考虑元素顺序）
 * 
 * 比较规则：
 * 1. 容器大小必须相等
 * 2. 对应位置的元素必须相等（使用operator==）
 * 
 * @tparam T 容器类型，必须满足is_iterable要求
 * @param t1 第一个容器
 * @param t2 第二个容器
 * @return 如果容器相等返回true，否则返回false
 */
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

/**
 * @brief 检查两个容器是否相等（不考虑元素顺序）
 * 
 * 比较规则：
 * 1. 容器大小必须相等
 * 2. 排序后对应位置的元素必须相等
 * 
 * @note 会修改输入容器的副本进行排序
 * 
 * @tparam T 容器类型，必须满足is_iterable要求
 * @param input_t1 第一个容器
 * @param input_t2 第二个容器
 * @return 如果容器相等返回true，否则返回false
 */
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

/**
 * @brief 检查两个映射是否相等
 * 
 * 比较规则：
 * 1. 映射大小必须相等
 * 2. 所有键必须相同
 * 3. 对应键的值必须相等
 * 
 * @tparam T 映射类型，必须满足is_map要求
 * @param map1 第一个映射
 * @param map2 第二个映射
 * @return 如果映射相等返回true，否则返回false
 */
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
