// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

/**
 * @file channel_manager.cc
 * @brief 通道管理器的实现文件
 * @details 该文件实现了通道管理器的核心功能，包括：
 *          - YAML配置的序列化和反序列化
 *          - 通道后端的注册和管理
 *          - 发布/订阅过滤器的注册和管理
 *          - 本地通道后端的初始化
 *          - 调试日志过滤器的注册
 */

#include "core/channel/channel_manager.h"
#include "core/channel/channel_backend_tools.h"
#include "core/channel/local_channel_backend.h"

namespace YAML {

/**
 * @brief YAML转换器特化，用于ChannelManager::Options的序列化和反序列化
 * @details 实现了Options结构体与YAML配置之间的双向转换：
 *          - encode: 将Options对象转换为YAML节点
 *          - decode: 将YAML节点解析为Options对象
 */
template <>
struct convert<aimrt::runtime::core::channel::ChannelManager::Options> {
  using Options = aimrt::runtime::core::channel::ChannelManager::Options;

  /**
   * @brief 将Options对象编码为YAML节点
   * @details 将以下内容编码为YAML格式：
   *          - backends: 后端配置列表
   *          - pub_topics_options: 发布主题配置
   *          - sub_topics_options: 订阅主题配置
   * @param rhs 要编码的Options对象
   * @return 编码后的YAML节点
   */
  static Node encode(const Options& rhs) {
    Node node;

    node["backends"] = YAML::Node();
    for (const auto& backend : rhs.backends_options) {
      Node backend_options_node;
      backend_options_node["type"] = backend.type;
      backend_options_node["options"] = backend.options;
      node["backends"].push_back(backend_options_node);
    }

    node["pub_topics_options"] = YAML::Node();
    for (const auto& pub_topic_options : rhs.pub_topics_options) {
      Node pub_topic_options_node;
      pub_topic_options_node["topic_name"] = pub_topic_options.topic_name;
      pub_topic_options_node["enable_backends"] = pub_topic_options.enable_backends;
      pub_topic_options_node["enable_filters"] = pub_topic_options.enable_filters;
      node["pub_topics_options"].push_back(pub_topic_options_node);
    }

    node["sub_topics_options"] = YAML::Node();
    for (const auto& sub_topic_options : rhs.sub_topics_options) {
      Node sub_topic_options_node;
      sub_topic_options_node["topic_name"] = sub_topic_options.topic_name;
      sub_topic_options_node["enable_backends"] = sub_topic_options.enable_backends;
      sub_topic_options_node["enable_filters"] = sub_topic_options.enable_filters;
      node["sub_topics_options"].push_back(sub_topic_options_node);
    }

    return node;
  }

  /**
   * @brief 将YAML节点解码为Options对象
   * @details 解析YAML节点中的：
   *          - backends: 创建后端配置列表
   *          - pub_topics_options: 解析发布主题配置
   *          - sub_topics_options: 解析订阅主题配置
   * @param node 要解码的YAML节点
   * @param rhs 解码后的Options对象
   * @return 解码是否成功
   */
  static bool decode(const Node& node, Options& rhs) {
    if (!node.IsMap()) return false;

    if (node["backends"] && node["backends"].IsSequence()) {
      for (const auto& backend_options_node : node["backends"]) {
        auto backend_options = Options::BackendOptions{
            .type = backend_options_node["type"].as<std::string>()};

        if (backend_options_node["options"])
          backend_options.options = backend_options_node["options"];
        else
          backend_options.options = YAML::Node(YAML::NodeType::Null);

        rhs.backends_options.emplace_back(std::move(backend_options));
      }
    }

    if (node["pub_topics_options"] && node["pub_topics_options"].IsSequence()) {
      for (const auto& pub_topic_options_node : node["pub_topics_options"]) {
        auto pub_topic_options = Options::PubTopicOptions{
            .topic_name = pub_topic_options_node["topic_name"].as<std::string>(),
            .enable_backends = pub_topic_options_node["enable_backends"].as<std::vector<std::string>>()};

        if (pub_topic_options_node["enable_filters"])
          pub_topic_options.enable_filters = pub_topic_options_node["enable_filters"].as<std::vector<std::string>>();

        rhs.pub_topics_options.emplace_back(std::move(pub_topic_options));
      }
    }

    if (node["sub_topics_options"] && node["sub_topics_options"].IsSequence()) {
      for (const auto& sub_topic_options_node : node["sub_topics_options"]) {
        auto sub_topic_options = Options::SubTopicOptions{
            .topic_name = sub_topic_options_node["topic_name"].as<std::string>(),
            .enable_backends = sub_topic_options_node["enable_backends"].as<std::vector<std::string>>()};

        if (sub_topic_options_node["enable_filters"])
          sub_topic_options.enable_filters = sub_topic_options_node["enable_filters"].as<std::vector<std::string>>();

        rhs.sub_topics_options.emplace_back(std::move(sub_topic_options));
      }
    }

    return true;
  }
};
}  // namespace YAML

namespace aimrt::runtime::core::channel {

/**
 * @brief 初始化通道管理器
 * @details 该函数执行以下初始化步骤：
 *          1. 注册本地通道后端和调试日志过滤器
 *          2. 检查状态并确保只初始化一次
 *          3. 从YAML配置加载选项
 *          4. 初始化通道注册表和设置日志器
 *          5. 配置通道后端管理器
 *          6. 根据配置初始化指定的后端
 *          7. 设置发布/订阅主题的后端和过滤器规则
 * 
 * @param options_node YAML配置节点
 * @throw std::runtime_error 当管理器已经初始化时抛出异常
 */
void ChannelManager::Initialize(YAML::Node options_node) {
  // 注册本地通道后端，用于进程内通信
  RegisterLocalChannelBackend();
  // 注册调试日志过滤器，用于记录消息的发布和订阅情况
  RegisterDebugLogFilter();

  // 确保通道管理器只被初始化一次，通过原子操作将状态从PreInit切换到Init
  AIMRT_CHECK_ERROR_THROW(
      std::atomic_exchange(&state_, State::kInit) == State::kPreInit,
      "Channel manager can only be initialized once.");

  // 如果配置节点有效且非空，则将其解析为Options结构体
  if (options_node && !options_node.IsNull())
    options_ = options_node.as<Options>();

  AIMRT_TRACE("~~~options_node: {}", YAML::Dump(options_node));

  // 创建通道注册表实例，用于管理所有的通道，是否也可以 channel_registry_manager_???
  channel_registry_ptr_ = std::make_unique<ChannelRegistry>();
  // 为通道注册表设置日志器
  channel_registry_ptr_->SetLogger(logger_ptr_);

  // 为通道后端管理器设置必要的组件
  channel_backend_manager_.SetLogger(logger_ptr_);  // 设置日志器
  channel_backend_manager_.SetChannelRegistry(channel_registry_ptr_.get());  // 设置通道注册表
  channel_backend_manager_.SetPublishFrameworkAsyncChannelFilterManager(&publish_filter_manager_);  // 设置发布过滤器管理器
  channel_backend_manager_.SetSubscribeFrameworkAsyncChannelFilterManager(&subscribe_filter_manager_);  // 设置订阅过滤器管理器

  // 用于存储已注册的通道后端名称
  std::vector<std::string> channel_backend_name_vec;

  // 根据配置初始化指定的backend
  for (auto& backend_options : options_.backends_options) {
    // 在已注册的后端中查找匹配类型的后端std::unique_ptr<ChannelBackendBase>
    auto finditr = std::find_if(
        channel_backend_vec_.begin(), channel_backend_vec_.end(),
        [&backend_options](const auto& ptr) {
          return ptr->Name() == backend_options.type;
        });

    // 如果找不到对应类型的后端，抛出异常
    AIMRT_CHECK_ERROR_THROW(finditr != channel_backend_vec_.end(),
                            "Invalid channel backend type '{}'",
                            backend_options.type);

    AIMRT_TRACE("~~~ (*finditr)->Name(): {}", (*finditr)->Name());
    // 为找到的后端设置通道注册表
    (*finditr)->SetChannelRegistry(channel_registry_ptr_.get());
    // 使用配置选项初始化后端
    (*finditr)->Initialize(backend_options.options);

    // 在通道后端管理器中注册该后端
    channel_backend_manager_.RegisterChannelBackend(finditr->get());

    // 将后端添加到已使用的后端列表中
    used_channel_backend_vec_.emplace_back(finditr->get());
    // 记录后端名称
    channel_backend_name_vec.emplace_back((*finditr)->Name());
  }

  // 设置发布规则
  std::vector<std::pair<std::string, std::vector<std::string>>> pub_backends_rules;  // 存储主题与后端的映射关系
  std::vector<std::pair<std::string, std::vector<std::string>>> pub_filters_rules;   // 存储主题与过滤器的映射关系
  for (const auto& item : options_.pub_topics_options) {
    // 验证每个发布主题配置的后端是否存在
    for (const auto& backend_name : item.enable_backends) {
      AIMRT_CHECK_ERROR_THROW(
          std::find(channel_backend_name_vec.begin(), channel_backend_name_vec.end(), backend_name) != channel_backend_name_vec.end(),
          "Invalid channel backend type '{}' for pub topic '{}'",
          backend_name, item.topic_name);
    }

    // 添加发布主题的后端规则和过滤器规则
    pub_backends_rules.emplace_back(item.topic_name, item.enable_backends);
    pub_filters_rules.emplace_back(item.topic_name, item.enable_filters);
  }
  // 设置发布主题的后端规则和过滤器规则
  channel_backend_manager_.SetPubTopicsBackendsRules(pub_backends_rules);
  channel_backend_manager_.SetPublishFiltersRules(pub_filters_rules);

  // 设置订阅规则
  std::vector<std::pair<std::string, std::vector<std::string>>> sub_backends_rules;  // 存储主题与后端的映射关系
  std::vector<std::pair<std::string, std::vector<std::string>>> sub_filters_rules;   // 存储主题与过滤器的映射关系
  for (const auto& item : options_.sub_topics_options) {
    // 验证每个订阅主题配置的后端是否存在
    for (const auto& backend_name : item.enable_backends) {
      AIMRT_CHECK_ERROR_THROW(
          std::find(channel_backend_name_vec.begin(), channel_backend_name_vec.end(), backend_name) != channel_backend_name_vec.end(),
          "Invalid channel backend type '{}' for sub topic '{}'",
          backend_name, item.topic_name);
    }

    // 添加订阅主题的后端规则和过滤器规则
    sub_backends_rules.emplace_back(item.topic_name, item.enable_backends);
    sub_filters_rules.emplace_back(item.topic_name, item.enable_filters);
  }
  // 设置订阅主题的后端规则和过滤器规则
  channel_backend_manager_.SetSubTopicsBackendsRules(sub_backends_rules);
  channel_backend_manager_.SetSubscribeFiltersRules(sub_filters_rules);

  // 初始化通道后端管理器
  channel_backend_manager_.Initialize();

  // 将解析后的选项写回配置节点
  options_node = options_;

  // 记录初始化完成的日志
  AIMRT_INFO("Channel manager init complete");
}

/**
 * @brief 启动通道管理器
 * @details 执行以下启动步骤：
 *          1. 检查状态确保从Init状态启动
 *          2. 启动通道后端管理器
 *          3. 设置通道代理启动标志
 * 
 * @throw std::runtime_error 当管理器状态不是Init时抛出异常
 */
void ChannelManager::Start() {
  AIMRT_CHECK_ERROR_THROW(
      std::atomic_exchange(&state_, State::kStart) == State::kInit,
      "Method can only be called when state is 'Init'.");

  channel_backend_manager_.Start();
  channel_handle_proxy_start_flag_.store(true);

  AIMRT_INFO("Channel manager start completed.");
}

/**
 * @brief 关闭通道管理器
 * @details 执行以下清理步骤：
 *          1. 检查是否已经关闭，避免重复关闭
 *          2. 清除所有通道代理包装器
 *          3. 关闭通道后端管理器
 *          4. 清除所有通道后端
 *          5. 重置通道注册表
 *          6. 清除发布/订阅过滤器
 *          7. 重置执行器函数
 */
void ChannelManager::Shutdown() {
  if (std::atomic_exchange(&state_, State::kShutdown) == State::kShutdown)
    return;

  AIMRT_INFO("Channel manager shutdown.");

  channel_handle_proxy_wrap_map_.clear();

  channel_backend_manager_.Shutdown();

  channel_backend_vec_.clear();

  channel_registry_ptr_.reset();

  subscribe_filter_manager_.Clear();
  publish_filter_manager_.Clear();

  get_executor_func_ = std::function<executor::ExecutorRef(std::string_view)>();
}

/**
 * @brief 生成初始化报告
 * @details 生成一个详细的报告，包含以下信息：
 *          1. 已注册的通道后端列表
 *          2. 发布过滤器列表
 *          3. 订阅过滤器列表
 *          4. 发布主题的详细信息（主题名、消息类型、模块、后端、过滤器）
 *          5. 订阅主题的详细信息（主题名、消息类型、模块、后端、过滤器）
 *          6. 各个后端的初始化报告
 * 
 * @return 包含报告信息的字符串对列表，每对包含一个标题和对应的内容
 * @throw std::runtime_error 当管理器状态不是Init时抛出异常
 */
std::list<std::pair<std::string, std::string>> ChannelManager::GenInitializationReport() const {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kInit,
      "Method can only be called when state is 'Init'.");

  std::vector<std::vector<std::string>> pub_topic_info_table =
      {{"topic", "msg type", "module", "backends", "filters"}};

  const auto& pub_topic_backend_info = channel_backend_manager_.GetPubTopicBackendInfo();
  const auto& pub_topic_index_map = channel_registry_ptr_->GetPubTopicIndexMap();

  for (const auto& pub_topic_index_itr : pub_topic_index_map) {
    auto topic_name = pub_topic_index_itr.first;
    auto pub_topic_backend_itr = pub_topic_backend_info.find(topic_name);
    AIMRT_CHECK_ERROR_THROW(pub_topic_backend_itr != pub_topic_backend_info.end(),
                            "Invalid channel registry info.");

    auto filter_name_vec = publish_filter_manager_.GetFilterNameVec(topic_name);

    for (const auto& item : pub_topic_index_itr.second) {
      std::vector<std::string> cur_topic_info(5);
      cur_topic_info[0] = topic_name;
      cur_topic_info[1] = item->info.msg_type;
      cur_topic_info[2] = item->info.module_name;
      cur_topic_info[3] = aimrt::common::util::JoinVec(pub_topic_backend_itr->second, ",");
      cur_topic_info[4] = aimrt::common::util::JoinVec(filter_name_vec, ",");
      pub_topic_info_table.emplace_back(std::move(cur_topic_info));
    }
  }

  std::vector<std::vector<std::string>> sub_topic_info_table =
      {{"topic", "msg type", "module", "backends", "filters"}};

  const auto& sub_topic_backend_info = channel_backend_manager_.GetSubTopicBackendInfo();
  const auto& sub_topic_index_map = channel_registry_ptr_->GetSubTopicIndexMap();

  for (const auto& sub_topic_index_itr : sub_topic_index_map) {
    auto topic_name = sub_topic_index_itr.first;
    auto sub_topic_backend_itr = sub_topic_backend_info.find(topic_name);
    AIMRT_CHECK_ERROR_THROW(sub_topic_backend_itr != sub_topic_backend_info.end(),
                            "Invalid channel registry info.");

    auto filter_name_vec = subscribe_filter_manager_.GetFilterNameVec(topic_name);

    for (const auto& item : sub_topic_index_itr.second) {
      std::vector<std::string> cur_topic_info(5);
      cur_topic_info[0] = topic_name;
      cur_topic_info[1] = item->info.msg_type;
      cur_topic_info[2] = item->info.module_name;
      cur_topic_info[3] = aimrt::common::util::JoinVec(sub_topic_backend_itr->second, ",");
      cur_topic_info[4] = aimrt::common::util::JoinVec(filter_name_vec, ",");
      sub_topic_info_table.emplace_back(std::move(cur_topic_info));
    }
  }

  std::vector<std::string> channel_backend_name_vec;
  channel_backend_name_vec.reserve(channel_backend_vec_.size());
  for (const auto& item : channel_backend_vec_)
    channel_backend_name_vec.emplace_back(item->Name());

  std::string channel_backend_name_vec_str;
  if (channel_backend_name_vec.empty()) {
    channel_backend_name_vec_str = "<empty>";
  } else {
    channel_backend_name_vec_str = "[ " + aimrt::common::util::JoinVec(channel_backend_name_vec, " , ") + " ]";
  }

  auto channel_pub_filter_name_vec = publish_filter_manager_.GetAllFiltersName();
  std::string channel_pub_filter_name_vec_str;
  if (channel_pub_filter_name_vec.empty()) {
    channel_pub_filter_name_vec_str = "<empty>";
  } else {
    channel_pub_filter_name_vec_str = "[ " + aimrt::common::util::JoinVec(channel_pub_filter_name_vec, " , ") + " ]";
  }

  auto channel_sub_filter_name_vec = subscribe_filter_manager_.GetAllFiltersName();
  std::string channel_sub_filter_name_vec_str;
  if (channel_sub_filter_name_vec.empty()) {
    channel_sub_filter_name_vec_str = "<empty>";
  } else {
    channel_sub_filter_name_vec_str = "[ " + aimrt::common::util::JoinVec(channel_sub_filter_name_vec, " , ") + " ]";
  }

  std::list<std::pair<std::string, std::string>> report{
      {"Channel Backend List", channel_backend_name_vec_str},
      {"Channel Pub Filter List", channel_pub_filter_name_vec_str},
      {"Channel Sub Filter List", channel_sub_filter_name_vec_str},
      {"Channel Pub Topic Info", aimrt::common::util::DrawTable(pub_topic_info_table)},
      {"Channel Sub Topic Info", aimrt::common::util::DrawTable(sub_topic_info_table)}};

  for (const auto& backend_ptr : used_channel_backend_vec_) {
    report.splice(report.end(), backend_ptr->GenInitializationReport());
  }

  return report;
}

/**
 * @brief 注册通道后端
 * @details 执行以下操作：
 *          1. 检查管理器状态是否为PreInit
 *          2. 验证后端指针的有效性
 *          3. 设置后端的日志器
 *          4. 将后端添加到后端向量中
 * 
 * @param channel_backend_ptr 通道后端的唯一指针
 * @throw std::runtime_error 当管理器状态不是PreInit时抛出异常
 * @throw std::runtime_error 当后端指针为空时抛出异常
 */
void ChannelManager::RegisterChannelBackend(
    std::unique_ptr<ChannelBackendBase>&& channel_backend_ptr) {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kPreInit,
      "Method can only be called when state is 'PreInit'.");

  channel_backend_vec_.emplace_back(std::move(channel_backend_ptr));
}

/**
 * @brief 注册获取执行器的函数
 * @details 用于注册一个函数，该函数负责根据名称获取对应的执行器
 *          执行器用于处理通道的异步操作和消息调度
 * @param get_executor_func 获取执行器的函数对象
 * @throw std::runtime_error 当管理器状态不是PreInit时抛出异常
 */
void ChannelManager::RegisterGetExecutorFunc(
    const std::function<executor::ExecutorRef(std::string_view)>& get_executor_func) {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kPreInit,
      "Method can only be called when state is 'PreInit'.");

  AIMRT_CHECK_ERROR_THROW(get_executor_func != nullptr, "Invalid get executor func.");

  get_executor_func_ = get_executor_func;
}

/**
 * @brief 获取通道句柄代理
 * @details 执行以下操作：
 *          1. 检查管理器状态是否为Init
 *          2. 检查模块信息是否有效
 *          3. 检查通道句柄代理是否已经存在
 *          4. 创建新的通道句柄代理包装器
 *          5. 返回通道句柄代理
 * 
 * @param module_info 模块详细信息
 * @return 通道句柄代理
 * @throw std::runtime_error 当管理器状态不是Init时抛出异常
 */
const ChannelHandleProxy& ChannelManager::GetChannelHandleProxy(
    const util::ModuleDetailInfo& module_info) {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kInit,
      "Method can only be called when state is 'Init'.");

  AIMRT_TRACE("ChannelHandleProxy_module_info.name '{}'.", module_info.name);

  auto itr = channel_handle_proxy_wrap_map_.find(module_info.name);
  if (itr != channel_handle_proxy_wrap_map_.end())
  {
    AIMRT_TRACE("emplace_ret.first->second->channel_handle_proxy_addr '{}'.", (void*)(&(itr->second->channel_handle_proxy)));

    return itr->second->channel_handle_proxy;
  }


  auto emplace_ret = channel_handle_proxy_wrap_map_.emplace(
      module_info.name,
      std::make_unique<ChannelHandleProxyWrap>(
          module_info.pkg_path,
          module_info.name,
          *logger_ptr_,
          channel_backend_manager_,
          passed_context_meta_keys_,
          channel_handle_proxy_start_flag_));
  AIMRT_TRACE("Create new channel handle proxy for module '{}'.", module_info.name);
  AIMRT_TRACE("emplace_ret.first->second->channel_handle_proxy_addr '{}'.", (void*)(&(emplace_ret.first->second->channel_handle_proxy)));

  return emplace_ret.first->second->channel_handle_proxy;
}

/**
 * @brief 注册发布过滤器
 * @details 注册一个过滤器用于处理发布消息
 *          过滤器可以对消息进行预处理、验证或转换
 * @param name 过滤器名称
 * @param filter 过滤器函数对象
 * @throw std::runtime_error 当管理器状态不是PreInit时抛出异常
 */
void ChannelManager::RegisterPublishFilter(std::string_view name, FrameworkAsyncChannelFilter&& filter) {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kPreInit,
      "Method can only be called when state is 'PreInit'.");
  publish_filter_manager_.RegisterFilter(name, std::move(filter));
}

/**
 * @brief 注册订阅过滤器
 * @details 注册一个过滤器用于处理订阅消息
 *          过滤器可以对接收到的消息进行过滤、转换或验证
 * @param name 过滤器名称
 * @param filter 过滤器函数对象
 * @throw std::runtime_error 当管理器状态不是PreInit时抛出异常
 */
void ChannelManager::RegisterSubscribeFilter(std::string_view name, FrameworkAsyncChannelFilter&& filter) {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kPreInit,
      "Method can only be called when state is 'PreInit'.");
  subscribe_filter_manager_.RegisterFilter(name, std::move(filter));
}

/**
 * @brief 添加传递上下文元数据键
 * @details 执行以下操作：
 *          1. 检查管理器状态是否为PreInit
 *          2. 验证键集合的有效性
 *          3. 添加传递上下文元数据键
 * 
 * @param keys 传递上下文元数据键集合
 * @throw std::runtime_error 当管理器状态不是PreInit时抛出异常
 */
void ChannelManager::AddPassedContextMetaKeys(const std::unordered_set<std::string>& keys) {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kPreInit,
      "Method can only be called when state is 'PreInit'.");
  passed_context_meta_keys_.insert(keys.begin(), keys.end());
}

/**
 * @brief 获取通道注册表
 * @details 执行以下操作：
 *          1. 检查管理器状态是否为Init
 *          2. 返回通道注册表指针
 * 
 * @return 通道注册表指针
 * @throw std::runtime_error 当管理器状态不是Init时抛出异常
 */
const ChannelRegistry* ChannelManager::GetChannelRegistry() const {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kInit,
      "Method can only be called when state is 'Init'.");
  return channel_registry_ptr_.get();
}

/**
 * @brief 获取已使用的通道后端
 * @details 执行以下操作：
 *          1. 检查管理器状态是否为Init
 *          2. 返回已使用的通道后端向量
 * 
 * @return 已使用的通道后端向量
 * @throw std::runtime_error 当管理器状态不是Init时抛出异常
 */
const std::vector<ChannelBackendBase*>& ChannelManager::GetUsedChannelBackend() const {
  AIMRT_CHECK_ERROR_THROW(
      state_.load() == State::kInit,
      "Method can only be called when state is 'Init'.");
  return used_channel_backend_vec_;
}

/**
 * @brief 注册本地通道后端
 * @details 执行以下操作：
 *          1. 创建本地通道后端
 *          2. 注册获取执行器函数
 *          3. 设置日志器
 *          4. 注册本地通道后端
 */
void ChannelManager::RegisterLocalChannelBackend() {
  // 创建本地通道后端
  std::unique_ptr<ChannelBackendBase> local_channel_backend_ptr =
      std::make_unique<LocalChannelBackend>();

  // 注册获取执行器函数，static_cast<> 用于强制转换，可参考 @20250131C++类&指针.md
  static_cast<LocalChannelBackend*>(local_channel_backend_ptr.get())
      ->RegisterGetExecutorFunc(get_executor_func_);

  // 设置日志器
  static_cast<LocalChannelBackend*>(local_channel_backend_ptr.get())
      ->SetLogger(logger_ptr_);

  RegisterChannelBackend(std::move(local_channel_backend_ptr));
}

/**
 * @brief 注册调试日志过滤器，用于记录消息的发布和订阅
 * @details 注册两个过滤器：
 *          1. 发布过滤器：记录消息发布时的详细信息
 *          2. 订阅过滤器：记录消息被订阅处理时的详细信息
 *          过滤器会尝试将消息序列化为JSON格式以便于调试
 */
void ChannelManager::RegisterDebugLogFilter() {
  // 注册发布消息的调试过滤器
  RegisterPublishFilter(
      "debug_log",
      [this](MsgWrapper& msg_wrapper, FrameworkAsyncChannelHandle&& h) {
        // 尝试将消息序列化为JSON格式
        auto buf_ptr = TrySerializeMsgWithCache(msg_wrapper, "json");

        if (buf_ptr) {
          auto msg_str = buf_ptr->JoinToString();
          // 记录完整的消息信息，包括消息内容
          AIMRT_INFO("Channel publish a new msg. msg type: {}, topic name: {}, context: {}, msg: {}",
                     msg_wrapper.info.msg_type,
                     msg_wrapper.info.topic_name,
                     msg_wrapper.ctx_ref.ToString(),
                     msg_str);
        } else {
          // 序列化失败时，记录基本信息
          AIMRT_INFO("Channel publish a new msg. msg type: {}, topic name: {}, context: {}",
                     msg_wrapper.info.msg_type,
                     msg_wrapper.info.topic_name,
                     msg_wrapper.ctx_ref.ToString());
        }

        // 继续消息处理链
        h(msg_wrapper);
      });

  // 注册订阅消息的调试过滤器
  RegisterSubscribeFilter(
      "debug_log",
      [this](MsgWrapper& msg_wrapper, FrameworkAsyncChannelHandle&& h) {
        // 尝试将消息序列化为JSON格式
        auto buf_ptr = TrySerializeMsgWithCache(msg_wrapper, "json");

        if (buf_ptr) {
          auto msg_str = buf_ptr->JoinToString();
          // 记录完整的消息信息，包括消息内容
          AIMRT_INFO("Channel subscriber handle a new msg. msg type: {}, topic name: {}, context: {}, msg: {}",
                     msg_wrapper.info.msg_type,
                     msg_wrapper.info.topic_name,
                     msg_wrapper.ctx_ref.ToString(),
                     msg_str);
        } else {
          // 序列化失败时，记录基本信息
          AIMRT_INFO("Channel subscriber handle a new msg. msg type: {}, topic name: {}, context: {}",
                     msg_wrapper.info.msg_type,
                     msg_wrapper.info.topic_name,
                     msg_wrapper.ctx_ref.ToString());
        }

        // 继续消息处理链
        h(msg_wrapper);
      });
}

}  // namespace aimrt::runtime::core::channel
