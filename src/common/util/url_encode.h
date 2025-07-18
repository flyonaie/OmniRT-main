// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <string>
#include <string_view>

namespace aimrt::common::util {

/**
 * @brief 将数字转换为十六进制字符
 * 
 * @param[in] x 要转换的数字(0-15)
 * @param[in] up true表示转换为大写字母，false表示转换为小写字母
 * @return unsigned char 转换后的十六进制字符
 */
inline unsigned char ToHex(unsigned char x, bool up) {
  return x > 9 ? x + (up ? 'A' : 'a') - 10 : (x + '0');
}

/**
 * @brief 将十六进制字符转换为数字
 * 
 * @param[in] x 十六进制字符('0'-'9', 'A'-'F', 'a'-'f')
 * @return unsigned char 转换后的数字(0-15)，如果输入无效则返回0
 */
inline unsigned char FromHex(unsigned char x) {
  if (x >= '0' && x <= '9') {
    return x - '0';
  }
  if (x >= 'A' && x <= 'F') {
    return x - 'A' + 10;
  }
  if (x >= 'a' && x <= 'f') {
    return x - 'a' + 10;
  }
  return 0;
}

/**
 * @brief URL编码，将字符串转换为URL安全的格式
 * 
 * 编码规则:
 * 1. 字母数字保持不变
 * 2. 特殊字符('-', '_', '.', '~')保持不变
 * 3. 空格转换为'+'
 * 4. 其他字符转换为%XX格式，XX为字符的十六进制ASCII码
 * 
 * @param[in] str 需要编码的字符串
 * @param[in] up 是否使用大写十六进制字母，默认为true
 * @return std::string 编码后的字符串
 */
inline std::string UrlEncode(std::string_view str, bool up = true) {
  std::string ret_str;
  ret_str.reserve(str.length() * 3);

  for (unsigned char c : str) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      ret_str += static_cast<char>(c);
    } else if (c == ' ') {
      ret_str += '+';
    } else {
      ret_str += '%';
      ret_str += static_cast<char>(ToHex(c >> 4, up));
      ret_str += static_cast<char>(ToHex(c & 0xF, up));
    }
  }
  return ret_str;
}

/**
 * @brief URL解码，将URL编码的字符串转换回原始格式
 * 
 * 解码规则:
 * 1. '+'转换为空格
 * 2. %XX格式转换为对应的字符，XX为十六进制ASCII码
 * 3. 其他字符保持不变
 * 
 * @param[in] str 需要解码的字符串
 * @return std::string 解码后的字符串
 */
inline std::string UrlDecode(std::string_view str) {
  std::string ret_str;
  ret_str.reserve(str.length());

  for (size_t i = 0; i < str.length(); ++i) {
    if (str[i] == '+') {
      ret_str += ' ';
    } else if (str[i] == '%') {
      if (i + 2 < str.length()) {
        ret_str += static_cast<char>((FromHex(str[i + 1]) << 4) | FromHex(str[i + 2]));
        i += 2;
      } else {
        break;
      }
    } else {
      ret_str += str[i];
    }
  }

  return ret_str;
}

/**
 * @brief HTTP头部编码，将字符串转换为HTTP头部安全的格式
 * 
 * 编码规则:
 * 1. 字母数字保持不变
 * 2. 连字符'-'保持不变
 * 3. 其他字符转换为%XX格式，XX为字符的十六进制ASCII码
 * 
 * @param[in] str 需要编码的字符串
 * @param[in] up 是否使用大写十六进制字母，默认为true
 * @return std::string 编码后的字符串
 */
inline std::string HttpHeaderEncode(std::string_view str, bool up = true) {
  std::string ret_str;
  ret_str.reserve(str.length() * 3);

  for (unsigned char c : str) {
    if (std::isalnum(c) || c == '-') {
      ret_str += static_cast<char>(c);
    } else {
      ret_str += '%';
      ret_str += static_cast<char>(ToHex(c >> 4, up));
      ret_str += static_cast<char>(ToHex(c & 0xF, up));
    }
  }
  return ret_str;
}

/**
 * @brief HTTP头部解码，将HTTP头部编码的字符串转换回原始格式
 * 
 * 解码规则:
 * 1. %XX格式转换为对应的字符，XX为十六进制ASCII码
 * 2. 其他字符保持不变
 * 
 * @param[in] str 需要解码的字符串
 * @return std::string 解码后的字符串
 */
inline std::string HttpHeaderDecode(std::string_view str) {
  std::string ret_str;
  ret_str.reserve(str.length());

  for (size_t i = 0; i < str.length(); ++i) {
    if (str[i] == '%') {
      if (i + 2 < str.length()) {
        ret_str += static_cast<char>((FromHex(str[i + 1]) << 4) | FromHex(str[i + 2]));
        i += 2;
      } else {
        break;
      }
    } else {
      ret_str += str[i];
    }
  }
  return ret_str;
}

}  // namespace aimrt::common::util
