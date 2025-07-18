// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

/**
 * @file configurator_manager.cc
 * @brief AimRT运行时配置管理器的实现文件
 * @details 该配置管理器负责处理AimRT运行时的所有配置相关操作,主要功能包括:
 *   1. YAML配置文件的读取和解析
 *   2. 配置项的分发和管理
 *   3. 模块级别的配置代理管理
 *   4. 临时配置文件的生成和管理
 *   5. 配置状态的监控和报告
 */

#include "core/configurator/configurator_manager.h"

#include <cstdlib>
#include <fstream>

#include "core/util/yaml_tools.h"
#include "util/string_util.h"

namespace YAML {

/**
 * @brief 为ConfiguratorManager::Options实现YAML序列化和反序列化
 * @details 
 *   - encode: 将Options结构体转换为YAML节点
 *   - decode: 将YAML节点解析为Options结构体
 *   - 主要处理临时配置文件路径的转换
 */
template <>
struct convert<aimrt::runtime::core::configurator::ConfiguratorManager::Options> {
  using Options = aimrt::runtime::core::configurator::ConfiguratorManager::Options;

  /**
   * @brief 将Options结构体转换为YAML节点
   * @param rhs Options结构体的实例
   * @return YAML::Node YAML节点
   */
  static Node encode(const Options& rhs) {
    Node node;

    node["temp_cfg_path"] = rhs.temp_cfg_path.string();

    return node;
  }

  /**
   * @brief 将YAML节点解析为Options结构体
   * @param node YAML节点
   * @param rhs Options结构体的实例
   * @return bool 解析是否成功
   */
  static bool decode(const Node& node, Options& rhs) {
    if (!node.IsMap()) return false;

    if (node["temp_cfg_path"])
      rhs.temp_cfg_path = node["temp_cfg_path"].as<std::string>();

    return true;
  }
};
}  // namespace YAML

namespace aimrt::runtime::core::configurator {

/**
 * @brief 初始化配置管理器
 * @param cfg_file_path 配置文件路径,可以是相对路径或绝对路径
 * @throw 如果重复初始化或配置文件无法访问时抛出异常
 * @details 初始化过程:
 *   1. 状态检查: 确保只能初始化一次,从PreInit转为Init状态
 *   2. 配置文件处理:
 *      - 将路径转换为规范化的绝对路径
 *      - 打开并读取配置文件内容
 *      - 替换配置中的环境变量
 *   3. YAML节点初始化:
 *      - 创建原始配置节点(ori_root_options_node)
 *      - 创建运行时配置节点(root_options_node)
 *      - 创建用户配置节点(user_root_options_node)
 *   4. 确保配置中包含aimrt根节点
 *   5. 解析configurator专属配置
 *   6. 创建临时配置文件目录
 */
void ConfiguratorManager::Initialize(
    const std::filesystem::path& cfg_file_path) {
  AIMRT_INFO("ConfiguratorManager::Initialize start");
  
  AIMRT_CHECK_ERROR_THROW(
      std::atomic_exchange(&state_, State::kInit) == State::kPreInit,
      "Configurator manager can only be initialized once.");

  cfg_file_path_ = cfg_file_path;

  // TODO: yaml-cpp和插件一起使用时无法在结束时正常析构。这里先不析构
  ori_root_options_node_ptr_ = new YAML::Node();
  root_options_node_ptr_ = new YAML::Node();
  user_root_options_node_ptr_ = new YAML::Node();

  // 原始配置节点：保存从配置文件读取的初始值
  auto& ori_root_options_node = *ori_root_options_node_ptr_;
  // 运行时配置节点：程序实际使用的配置
  auto& root_options_node = *root_options_node_ptr_;
  // 用户配置节点：记录用户的配置修改
  auto& user_root_options_node = *user_root_options_node_ptr_;

  if (!cfg_file_path_.empty()) {
    // 1. std::filesystem::absolute: 将路径转换为绝对路径
    // 2. std::filesystem::canonical: 将绝对路径规范化（解析所有符号链接、移除./和../等）
    cfg_file_path_ = std::filesystem::canonical(std::filesystem::absolute(cfg_file_path_));
    AIMRT_INFO("Cfg file path: {}", cfg_file_path_.string());

    std::ifstream file_stream(cfg_file_path_);
    AIMRT_CHECK_ERROR_THROW(file_stream, "Can not open cfg file '{}'.", cfg_file_path_.string());

    // 创建字符串流用于存储文件内容
    std::stringstream file_data;
    // 将文件流的所有内容读入字符串流中
    file_data << file_stream.rdbuf();
    // 1. 调用ReplaceEnvVars替换配置文件中的环境变量(如${HOME})
    // 2. 将处理后的字符串解析为YAML节点,存储为原始配置
    ori_root_options_node = YAML::Load(aimrt::common::util::ReplaceEnvVars(file_data.str()));
    // 克隆一份原始配置作为用户配置,用于后续用户自定义修改
    user_root_options_node = YAML::Clone(ori_root_options_node);
  } else {
    AIMRT_INFO("AimRT start with no cfg file.");
  }

  // 检查配置中是否存在"aimrt"根节点
  // 如果不存在，则创建一个空的YAML节点作为"aimrt"配置的容器
  // 这确保了后续对aimrt相关配置的操作不会因节点不存在而失败
  if (!ori_root_options_node["aimrt"]) {
    ori_root_options_node["aimrt"] = YAML::Node();
  }

  root_options_node["aimrt"] = YAML::Node();

  YAML::Node configurator_options_node = GetAimRTOptionsNode("configurator");
  if (configurator_options_node && !configurator_options_node.IsNull())
    options_ = configurator_options_node.as<Options>();

  if (!(std::filesystem::exists(options_.temp_cfg_path) &&
        std::filesystem::is_directory(options_.temp_cfg_path))) {
    std::filesystem::create_directories(options_.temp_cfg_path);
  }

  configurator_options_node = options_;

  AIMRT_INFO("Configurator manager init complete");
}

/**
 * @brief 启动配置管理器
 * @throw 如果不是从Init状态启动则抛出异常
 * @details 
 *   - 将状态从Init转换为Start
 *   - 此时配置已经完全加载并可以被其他模块使用
 */
void ConfiguratorManager::Start() {
  AIMRT_CHECK_ERROR_THROW(
      std::atomic_exchange(&state_, State::kStart) == State::kInit,
      "Method can only be called when state is 'Init'.");

  AIMRT_INFO("Configurator manager start completed.");
}

/**
 * @brief 关闭配置管理器
 * @details 
 *   - 使用原子操作确保只执行一次关闭
 *   - 清理所有配置代理
 *   - 将状态设置为Shutdown
 */
void ConfiguratorManager::Shutdown() {
  if (std::atomic_exchange(&state_, State::kShutdown) == State::kShutdown)
    return;

  AIMRT_INFO("Configurator manager shutdown.");

  cfg_proxy_map_.clear();
}

/**
 * @brief 获取原始的根配置节点
 * @return YAML::Node 原始配置的根节点
 * @details 原始配置节点保存了从配置文件直接读取的未经处理的配置
 */
YAML::Node ConfiguratorManager::GetOriRootOptionsNode() const {
  return *ori_root_options_node_ptr_;
}

/**
 * @brief 获取运行时的根配置节点
 * @return YAML::Node 运行时配置的根节点
 * @details 运行时配置节点包含了经过处理和可能被修改的配置
 */
YAML::Node ConfiguratorManager::GetRootOptionsNode() const {
  return *root_options_node_ptr_;
}

/**
 * @brief 获取用户的根配置节点
 * @return YAML::Node 用户配置的根节点
 * @details 用户配置节点用于存储用户特定的配置修改
 */
YAML::Node ConfiguratorManager::GetUserRootOptionsNode() const {
  return *user_root_options_node_ptr_;
}

/**
 * @brief 获取指定模块的配置代理
 * @param module_info 模块的详细信息,包含模块名称和可选的配置文件路径
 * @return const ConfiguratorProxy& 配置代理的常量引用
 * @throw 如果不在Init状态下调用则抛出异常
 * @details 配置代理获取流程:
 *   1. 检查模块是否已有配置代理
 *   2. 如果module_info指定了配置文件路径:
 *      - 直接使用指定的配置文件创建代理
 *   3. 如果根配置中包含该模块的配置:
 *      - 提取模块配置
 *      - 生成临时配置文件
 *      - 使用临时配置文件创建代理
 *   4. 如果都没有,返回默认配置代理
 */
const ConfiguratorProxy& ConfiguratorManager::GetConfiguratorProxy(
    const util::ModuleDetailInfo& module_info) {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kInit,
      "Method can only be called when state is 'Init'.");

  AIMRT_TRACE("Get configurator proxy for module '{}'.", module_info.name);
  AIMRT_TRACE("module_info.cfg_file_path '{}'.", module_info.cfg_file_path);

  auto itr = cfg_proxy_map_.find(module_info.name);
  if (itr != cfg_proxy_map_.end()) 
  {
    AIMRT_TRACE("Configurator proxy for module '{}' already exists.", module_info.name);
    return *(itr->second);
  }

  // 如果直接指定了配置文件路径，则使用指定的
  if (!module_info.cfg_file_path.empty()) {
    auto emplace_ret = cfg_proxy_map_.emplace(
        module_info.name,
        std::make_unique<ConfiguratorProxy>(module_info.cfg_file_path));
    return *(emplace_ret.first->second);
  }

  auto& ori_root_options_node = *ori_root_options_node_ptr_;
  auto& root_options_node = *root_options_node_ptr_;

  // 如果根配置文件中有这个模块节点，则将内容生成到临时配置文件中
  AIMRT_TRACE("~~~ori_root_options_node: {}", YAML::Dump(ori_root_options_node));
  int flag = ori_root_options_node[module_info.name]?1:0;
  int flag2 = ori_root_options_node[module_info.name].IsNull()?1:0;
  AIMRT_TRACE("~~~flag: {}", flag);
  AIMRT_TRACE("~~~flag2: {}", flag2);

  if (ori_root_options_node[module_info.name] &&
      !ori_root_options_node[module_info.name].IsNull()) {
    root_options_node[module_info.name] =
        ori_root_options_node[module_info.name];

    std::filesystem::path temp_cfg_file_path =
        options_.temp_cfg_path /
        ("temp_cfg_file_for_" + module_info.name + ".yaml");
    std::ofstream ofs;
    ofs.open(temp_cfg_file_path, std::ios::trunc);
    ofs << root_options_node[module_info.name];
    ofs.flush();
    ofs.clear();
    ofs.close();

    auto emplace_ret = cfg_proxy_map_.emplace(
        module_info.name,
        std::make_unique<ConfiguratorProxy>(temp_cfg_file_path.string()));
    return *(emplace_ret.first->second);
  }

  AIMRT_TRACE("~~~default_cfg_proxy_p: {}", (void*)&default_cfg_proxy_);

  return default_cfg_proxy_;
}

/**
 * @brief 获取AimRT特定配置项
 * @param key 配置项的键名
 * @return YAML::Node 对应的配置节点
 * @throw 如果不在Init状态下调用则抛出异常
 * @details 
 *   - 从aimrt命名空间下获取指定key的配置
 *   - 同时更新原始配置和运行时配置
 */
YAML::Node ConfiguratorManager::GetAimRTOptionsNode(std::string_view key) {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kInit,
      "Method can only be called when state is 'Init'.");

  auto& ori_root_options_node = *ori_root_options_node_ptr_;
  auto& root_options_node = *root_options_node_ptr_;

  // 检查配置中是否存在"aimrt"根节点
  // 如果不存在，则创建一个空的YAML节点作为"aimrt"配置的容器
  // 这确保了后续对aimrt相关配置的操作不会因节点不存在而失败
  if (!ori_root_options_node["aimrt"]) {
    ori_root_options_node["aimrt"] = YAML::Node();
  }

  return root_options_node["aimrt"][key] = ori_root_options_node["aimrt"][key];
}

/**
 * @brief 生成配置初始化报告
 * @return std::list<std::pair<std::string, std::string>> 配置报告列表
 * @throw 如果不在Init状态下调用则抛出异常
 * @details 报告内容包括:
 *   1. 配置文件的绝对路径
 *   2. AimRT核心配置的YAML dump
 *   3. 配置检查的警告信息(如果有)
 *   - 用于调试和验证配置是否正确加载
 */
std::list<std::pair<std::string, std::string>> ConfiguratorManager::GenInitializationReport() const {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kInit,
      "Method can only be called when state is 'Init'.");

  auto check_msg = util::CheckYamlNodes(
      GetRootOptionsNode()["aimrt"],
      GetUserRootOptionsNode()["aimrt"],
      "aimrt");

  std::list<std::pair<std::string, std::string>> report{
      {"AimRT Cfg File Path", cfg_file_path_.string()},
      {"AimRT Core Option", YAML::Dump((*root_options_node_ptr_)["aimrt"])}};

  if (!check_msg.empty()) {
    report.emplace_back(
        std::pair<std::string, std::string>{"Configuration Warning", check_msg});
  }

  return report;
}

}  // namespace aimrt::runtime::core::configurator