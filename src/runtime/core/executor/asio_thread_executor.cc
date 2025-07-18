/**
 * @file asio_thread_executor.cc
 * @brief 基于Asio的线程执行器实现
 * @details 该执行器提供了基于asio的异步任务处理能力，支持：
 *          - 多线程并行处理
 *          - 任务队列监控
 *          - CPU绑定和调度策略设置
 *          - 定时任务执行
 * @copyright Copyright (c) 2023, AgiBot Inc.
 */

#include "core/executor/asio_thread_executor.h"
#include "core/util/thread_tools.h"
#include "util/macros.h"

namespace YAML {
/**
 * @brief YAML转换特化模板，用于AsioThreadExecutor的Options配置
 * @details 提供了Options结构体与YAML节点之间的双向转换能力
 */
template <>
struct convert<aimrt::runtime::core::executor::AsioThreadExecutor::Options> {
  using Options = aimrt::runtime::core::executor::AsioThreadExecutor::Options;

  /**
   * @brief 将Options对象编码为YAML节点
   * @param rhs 要编码的Options对象
   * @return 编码后的YAML节点
   * @details 编码的配置项包括：
   *          - thread_num: 线程数量
   *          - thread_sched_policy: 线程调度策略
   *          - thread_bind_cpu: CPU绑定设置
   *          - timeout_alarm_threshold_us: 超时告警阈值
   *          - queue_threshold: 队列长度阈值
   */
  static Node encode(const Options& rhs) {
    Node node;

    node["thread_num"] = rhs.thread_num;
    node["thread_sched_policy"] = rhs.thread_sched_policy;
    node["thread_bind_cpu"] = rhs.thread_bind_cpu;
    node["timeout_alarm_threshold_us"] = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            rhs.timeout_alarm_threshold_us)
            .count());
    node["queue_threshold"] = rhs.queue_threshold;

    return node;
  }

  /**
   * @brief 从YAML节点解码为Options对象
   * @param node YAML配置节点
   * @param rhs 解码后的Options对象
   * @return 解码是否成功
   * @details 支持部分配置项缺失的情况，未配置的项将保持默认值
   */
  static bool decode(const Node& node, Options& rhs) {
    if (!node.IsMap()) return false;

    if (node["thread_num"]) rhs.thread_num = node["thread_num"].as<uint32_t>();
    if (node["thread_sched_policy"])
      rhs.thread_sched_policy = node["thread_sched_policy"].as<std::string>();
    if (node["thread_bind_cpu"])
      rhs.thread_bind_cpu = node["thread_bind_cpu"].as<std::vector<uint32_t>>();
    if (node["timeout_alarm_threshold_us"])
      rhs.timeout_alarm_threshold_us = std::chrono::microseconds(
          node["timeout_alarm_threshold_us"].as<uint64_t>());
    if (node["queue_threshold"])
      rhs.queue_threshold = node["queue_threshold"].as<uint32_t>();

    return true;
  }
};
}  // namespace YAML

namespace aimrt::runtime::core::executor {

/**
 * @brief 初始化异步线程执行器
 * @param name 执行器名称
 * @param options_node YAML配置节点
 * @throw 如果重复初始化会抛出异常
 * @details 初始化过程包括：
 *          1. 状态检查和设置
 *          2. 配置参数加载
 *          3. 任务队列阈值设置
 *          4. IO上下文初始化
 *          5. 工作线程创建和配置
 */
void AsioThreadExecutor::Initialize(std::string_view name,
                                    YAML::Node options_node) {
  // 使用原子操作确保线程安全的状态转换
  AIMRT_CHECK_ERROR_THROW(
      std::atomic_exchange(&state_, State::kInit) == State::kPreInit,
      "AsioThreadExecutor can only be initialized once.");

  // 设置执行器名称和加载配置
  name_ = std::string(name);
  if (options_node && !options_node.IsNull())
    options_ = options_node.as<Options>();

  // 初始化任务队列监控阈值
  queue_threshold_ = options_.queue_threshold;
  queue_warn_threshold_ = queue_threshold_ * 0.95;  // 设置95%作为警告阈值

  // 验证线程数配置
  AIMRT_CHECK_ERROR_THROW(
      options_.thread_num > 0,
      "Invalide asio thread executor options, thread num is zero.");

  // 创建asio的IO上下文，用于事件循环
  io_ptr_ = std::make_unique<asio::io_context>(options_.thread_num);
  work_guard_ptr_ = std::make_unique<
      asio::executor_work_guard<asio::io_context::executor_type>>(
      io_ptr_->get_executor());

  // 初始化线程存储
  thread_id_vec_.resize(options_.thread_num);

  // 创建并配置工作线程
  for (uint32_t ii = 0; ii < options_.thread_num; ++ii) {
    // 在线程容器中创建新的线程，并传入一个lambda函数作为线程的执行函数
    // 线程在这行代码创建执行完后就立即开始运行了
    // [this, ii]表示lambda表达式捕获当前对象指针和循环索引，使其在线程函数内部可访问
    threads_.emplace_back([this, ii] {
      // 记录线程ID，用于后续判断任务是否在当前执行器中执行
      // 当线程开始运行时，使用std::this_thread::get_id()来记录实际的线程ID
      thread_id_vec_[ii] = std::this_thread::get_id();

      // 设置线程名称，方便调试
      std::string threadname = name_;
      if (options_.thread_num > 1)
        threadname = threadname + "." + std::to_string(ii);

      try {
        util::SetNameForCurrentThread(threadname);
        util::BindCpuForCurrentThread(options_.thread_bind_cpu);
        util::SetCpuSchedForCurrentThread(options_.thread_sched_policy);
      } catch (const std::exception& e) {
        AIMRT_WARN("Set thread policy for asio thread executor '{}' get exception, {}",
                   Name(), e.what());
      }

      // 核心事件循环：只要还有事件就继续运行
      try {
        while (io_ptr_->run_one()) {
          --queue_task_num_;
        }
      } catch (const std::exception& e) {
        AIMRT_FATAL("Asio thread executor '{}' run loop get exception, {}",
                    Name(), e.what());
      }

      //当线程结束时，使用std::thread::id()来重置线程ID为无效状态
      thread_id_vec_[ii] = std::thread::id();
    });
  }

  options_node = options_;
}

/**
 * @brief 启动执行器
 * @throw 如果不在Init状态下调用会抛出异常
 * @details 通过原子操作确保状态安全转换为Start状态
 */
void AsioThreadExecutor::Start() {
  AIMRT_CHECK_ERROR_THROW(
      std::atomic_exchange(&state_, State::kStart) == State::kInit,
      "Method can only be called when state is 'Init'.");
}

/**
 * @brief 关闭执行器，停止所有工作线程
 * @details 关闭流程：
 *          1. 原子操作确保只执行一次关闭
 *          2. 移除work_guard允许事件循环退出
 *          3. 等待所有线程完成并清理资源
 */
void AsioThreadExecutor::Shutdown() {
  if (std::atomic_exchange(&state_, State::kShutdown) == State::kShutdown)
    return;

  if (work_guard_ptr_) work_guard_ptr_->reset();

  for (auto itr = threads_.begin(); itr != threads_.end();) {
    if (itr->joinable()) itr->join();
    threads_.erase(itr++);
  }
}

/**
 * @brief 检查当前线程是否属于该执行器
 * @return true 如果当前线程是执行器的工作线程之一
 * @return false 如果当前线程不属于该执行器或发生异常
 * @details 通过比对当前线程ID是否在执行器的线程ID列表中来判断
 */
bool AsioThreadExecutor::IsInCurrentExecutor() const noexcept {
  try {
    auto finditr = std::find(thread_id_vec_.begin(), thread_id_vec_.end(), std::this_thread::get_id());
    return (finditr != thread_id_vec_.end());
  } catch (const std::exception& e) {
    AIMRT_ERROR("{}", e.what());
  }
  return false;
}

/**
 * @brief 执行一个异步任务
 * @param task 要执行的任务（右值引用）
 * @note 如果任务队列已满或执行器状态不正确，任务将被丢弃
 * @details 执行流程：
 *          1. 检查执行器状态
 *          2. 验证任务队列容量
 *          3. 必要时发出队列接近阈值警告
 *          4. 将任务提交到asio事件循环
 */
void AsioThreadExecutor::Execute(aimrt::executor::Task&& task) noexcept {
  if (omnirt_unlikely(state_.load() != State::kInit && state_.load() != State::kStart)) {
    fprintf(stderr,
            "Asio thread executor '%s' can only execute task when state is 'Init' or 'Start'.\n",
            name_.c_str());
    return;
  }

  uint32_t cur_queue_task_num = ++queue_task_num_;

  if (omnirt_unlikely(cur_queue_task_num > queue_threshold_)) {
    fprintf(stderr,
            "The number of tasks in the asio thread executor '%s' has reached the threshold '%u', the task will not be delivered.\n",
            name_.c_str(), queue_threshold_);
    --queue_task_num_;
    return;
  }

  if (omnirt_unlikely(cur_queue_task_num > queue_warn_threshold_)) {
    fprintf(stderr,
            "The number of tasks in the asio thread executor '%s' is about to reach the threshold: '%u / %u'.\n",
            name_.c_str(), cur_queue_task_num, queue_threshold_);
  }

  try {
    asio::post(*io_ptr_, std::move(task));
  } catch (const std::exception& e) {
    fprintf(stderr, "Asio thread executor '%s' execute Task get exception: %s\n", name_.c_str(), e.what());
  }
}

/**
 * @brief 在指定时间点执行任务
 * @param tp 计划执行的时间点
 * @param task 要执行的任务（右值引用）
 * @note 如果任务队列已满或执行器状态不正确，任务将被丢弃
 * @note 如果执行延迟超过阈值，将产生警告
 * @details 定时执行流程：
 *          1. 状态和队列检查
 *          2. 创建定时器
 *          3. 设置执行时间点
 *          4. 注册异步等待回调
 *          5. 任务执行完成后检查延迟情况
 */
void AsioThreadExecutor::ExecuteAt(
    std::chrono::system_clock::time_point tp, aimrt::executor::Task&& task) noexcept {
  /**
   * @brief 检查执行器状态
   * @details 只有在Init或Start状态下才能执行任务
   */
  if (omnirt_unlikely(state_.load() != State::kInit && state_.load() != State::kStart)) {
    fprintf(stderr,
            "Asio thread executor '%s' can only execute task when state is 'Init' or 'Start'.\n",
            name_.c_str());
    return;
  }

  /**
   * @brief 任务队列计数和阈值检查
   * @details 原子递增任务计数，并检查是否超过阈值
   */
  uint32_t cur_queue_task_num = ++queue_task_num_;

  // 超过队列阈值，拒绝任务
  if (omnirt_unlikely(cur_queue_task_num > queue_threshold_)) {
    fprintf(stderr,
            "The number of tasks in the asio thread executor '%s' has reached the threshold '%u', the task will not be delivered.\n",
            name_.c_str(), queue_threshold_);
    --queue_task_num_;
    return;
  }

  // 接近队列阈值，发出警告
  if (omnirt_unlikely(cur_queue_task_num > queue_warn_threshold_)) {
    fprintf(stderr,
            "The number of tasks in the asio thread executor '%s' is about to reach the threshold: '%u / %u'.\n",
            name_.c_str(), cur_queue_task_num, queue_threshold_);
  }

  try {
    /**
     * @brief 创建定时器并设置回调
     * @details 
     * 1. 创建system_timer智能指针，确保定时器资源自动释放
     * 2. 设置定时器触发时间点
     * 3. 注册异步等待回调函数
     * 4. 当定时器到期时，会执行lambda表达式中的代码
     */
    auto timer_ptr = std::make_shared<asio::system_timer>(*io_ptr_);
    timer_ptr->expires_at(tp);
    // asio库的异步等待函数，用于设置定时器到期后要执行的回调函数
    timer_ptr->async_wait([this, timer_ptr,
                           task{std::move(task)}](asio::error_code ec) {
      /**
       * @brief 定时器回调处理
       * @details 
       * 1. 检查定时器错误
       * 2. 计算实际执行延迟
       * 3. 执行用户任务
       * 4. 检查执行延迟是否超过阈值
       */
      if (omnirt_unlikely(ec)) {
        AIMRT_ERROR("Asio thread executor '{}' timer get err, code '{}', msg: {}",
                    Name(), ec.value(), ec.message());
        return;
      }

      // 计算实际执行延迟时间
      auto dif_time = std::chrono::system_clock::now() - timer_ptr->expiry();

      // 执行用户任务
      task();

      // 检查执行延迟是否超过阈值
      AIMRT_CHECK_WARN(
          dif_time <= options_.timeout_alarm_threshold_us,
          "Asio thread executor '{}' timer delay too much, error time value '{}', require '{}'. "
          "Perhaps the CPU load is too high",
          Name(), std::chrono::duration_cast<std::chrono::microseconds>(dif_time),
          options_.timeout_alarm_threshold_us);
    });
  } catch (const std::exception& e) {
    fprintf(stderr, "Asio thread executor '%s' execute Task get exception: %s\n", name_.c_str(), e.what());
  }
}

}  // namespace aimrt::runtime::core::executor
