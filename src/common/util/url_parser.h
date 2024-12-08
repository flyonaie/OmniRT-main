// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <optional>
#include <regex>
#include <sstream>
#include <string>

namespace aimrt::common::util {

/**
 * @brief url元素
 *
 * @tparam StringType
 */
template <class StringType = std::string>
struct Url {
  StringType protocol;  // 协议
  StringType host;      // host
  StringType service;   // 端口
  StringType path;      // 路径
  StringType query;     // 参数
  StringType fragment;  // 额外信息
};

/**
 * @brief url解析
 * @note url结构：[protocol://][host][:service][path][?query][#fragment]
 * @param url_str url字符串
 * @return std::optional<UrlView> url结构，nullopt则代表解析失败
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
 * @brief url拼接
 * @note url结构：[protocol://][host][:service][path][?query][#fragment]
 * @param url url结构
 * @return std::string url字符串
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