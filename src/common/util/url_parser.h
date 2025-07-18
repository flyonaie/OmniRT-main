// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <optional>
#include <regex>
#include <sstream>
#include <string>

namespace aimrt::common::util {

/**
 * @brief URL结构体，用于存储URL的各个组成部分
 * @details 将URL分解为协议、主机名、端口、路径、查询参数和片段等组成部分
 * 
 * @tparam StringType 字符串类型，默认为std::string
 */
template <class StringType = std::string>
struct Url {
  StringType protocol;  // 协议，如http、https等
  StringType host;      // 主机名，如www.example.com
  StringType service;   // 服务端口，如80、443等
  StringType path;      // 资源路径，如/path/to/resource
  StringType query;     // 查询参数，如key1=value1&key2=value2
  StringType fragment;  // URL片段标识符，以#开头的部分
};

/**
 * @brief 解析URL字符串为URL结构体
 * @details 使用正则表达式将URL字符串解析为各个组成部分，支持标准URL格式
 * 
 * @tparam StringType 字符串类型，默认为std::string
 * @param url_str 需要解析的URL字符串
 * @return std::optional<Url<StringType>> 解析成功返回URL结构体，失败返回std::nullopt
 */
template <class StringType = std::string>
std::optional<Url<StringType>> ParseUrl(const std::string& url_str) {
  std::regex url_regex(
      R"(^(([^:\/?#]+)://)?(([^\/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?)",
      std::regex::ECMAScript);
  std::match_results<std::string::const_iterator> url_match_result;

  if (!std::regex_match(url_str.begin(), url_str.end(), url_match_result, url_regex))
    return std::nullopt;

  Url<StringType> url;
  if (url_match_result[2].matched)
    url.protocol = StringType(url_match_result[2].str());
  if (url_match_result[4].matched) {
    StringType auth(url_match_result[4].str());
    size_t pos = auth.find_first_of(':');
    if (pos != StringType::npos) {
      url.host = auth.substr(0, pos);
      url.service = auth.substr(pos + 1);
    } else {
      url.host = auth;
    }
  }
  if (url_match_result[5].matched)
    url.path = StringType(url_match_result[5].str());
  if (url_match_result[7].matched)
    url.query = StringType(url_match_result[7].str());
  if (url_match_result[9].matched)
    url.fragment = StringType(url_match_result[9].str());

  return std::optional<Url<StringType>>{url};
}

/**
 * @brief 将URL结构体转换为URL字符串
 * @details 根据URL结构体中的各个组成部分，按照标准URL格式拼接成完整的URL字符串
 * 
 * @tparam StringType 字符串类型，默认为std::string
 * @param url URL结构体，包含URL的各个组成部分
 * @return std::string 拼接后的完整URL字符串
 */
template <class StringType = std::string>
std::string JoinUrl(const Url<StringType>& url) {
  std::stringstream ss;

  if (!url.protocol.empty()) ss << url.protocol << "://";
  if (!url.host.empty()) ss << url.host;
  if (!url.service.empty()) ss << ":" << url.service;
  if (!url.path.empty()) {
    if (url.path[0] != '/') ss << '/';
    ss << url.path;
  }
  if (!url.query.empty()) ss << '?' << url.query;
  if (!url.fragment.empty()) ss << '#' << url.fragment;

  return ss.str();
}

}  // namespace aimrt::common::util