/**
 * @file native_thread_executor.cc
 * @brief 基于C++11标准库的线程执行器实现
 * @details 该执行器提供了基于标准库的异步任务处理能力，支持：
 *          - 多线程并行处理
 *          - 任务队列监控
 *          - CPU绑定和调度策略设置
 *          - 定时任务执行
 * @copyright Copyright (c) 2023, AgiBot Inc.
 */

#include "core/executor/native_thread_executor.h"
#include "core/util/thread_tools.h"
#include "util/macros.h"

#include <algorithm>
#include <chrono>
#include <functional>

namespace YAML {
/**
 * @brief YAML转换特化模板，用于NativeThreadExecutor的Options配置
 * @details 提供了Options结构体与YAML节点之间的双向转换能力
 */
template <>
struct convert<aimrt::runtime::core::executor::NativeThreadExecutor::Options> {
  using Options = aimrt::runtime::core::executor::NativeThreadExecutor::Options;

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
 * @brief 析构函数，确保线程安全关闭
 */
NativeThreadExecutor::~NativeThreadExecutor() {
  // 确保在析构时关闭执行器
  if (GetState() != State::kShutdown) {
    Shutdown();
  }
}

/**
 * @brief 初始化线程执行器
 * @param name 执行器名称
 * @param options_node YAML配置节点
 * @throw 如果重复初始化会抛出异常
 * @details 初始化过程包括：
 *          1. 状态检查和设置
 *          2. 配置参数加载
 *          3. 任务队列阈值设置
 */
void NativeThreadExecutor::Initialize(std::string_view name,
                                     YAML::Node options_node) {
  // 使用原子操作确保线程安全的状态转换
  AIMRT_CHECK_ERROR_THROW(
      std::atomic_exchange(&state_, State::kInit) == State::kPreInit,
      "NativeThreadExecutor can only be initialized once.");

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
      "Invalid native thread executor options, thread num is zero.");

  // 初始化运行标志
  running_ = false;

  // 创建工作线程
  thread_id_vec_.clear();
  for (uint32_t i = 0; i < options_.thread_num; ++i) {
    threads_.emplace_back([this, i] { WorkerThread(i); });
  }
}

/**
 * @brief 启动执行器
 * @throw 如果不在Init状态下调用会抛出异常
 * @details 通过原子操作确保状态安全转换为Start状态，并启动工作线程和定时器线程
 */
void NativeThreadExecutor::Start() {
  // 确保只能从Init状态启动
  AIMRT_CHECK_ERROR_THROW(
      std::atomic_exchange(&state_, State::kStart) == State::kInit,
      "NativeThreadExecutor can only start from Init state.");

  // 设置运行标志
  running_ = true;

}

/**
 * @brief 关闭执行器，停止所有工作线程
 * @details 关闭流程：
 *          1. 原子操作确保只执行一次关闭
 *          2. 设置停止标志
 *          3. 唤醒所有等待的线程
 *          4. 等待所有线程完成并清理资源
 */
void NativeThreadExecutor::Shutdown() {
  // 只有从Start或Init状态才能关闭
  State expected = State::kStart;
  if (!state_.compare_exchange_strong(expected, State::kShutdown)) {
    expected = State::kInit;
    if (!state_.compare_exchange_strong(expected, State::kShutdown)) {
      // 已经关闭或还未初始化，直接返回
      return;
    }
  }

  // 设置停止标志
  running_ = false;

  // 唤醒所有等待的工作线程
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    queue_condition_.notify_all();
  }

  // 唤醒定时器线程
  {
    std::lock_guard<std::mutex> lock(timer_mutex_);
    timer_condition_.notify_all();
  }

  // 等待所有线程结束
  for (auto& t : threads_) {
    if (t.joinable()) {
      t.join();
    }
  }
  threads_.clear();

  // 等待定时器线程结束
  if (timer_thread_ && timer_thread_->joinable()) {
    timer_thread_->join();
    timer_thread_.reset();
  }

  // 清空任务队列
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    task_queue_.clear();
    queue_task_num_ = 0;
  }

  // 清空定时器任务
#if 0
  {
    std::lock_guard<std::mutex> lock(timer_mutex_);
    timer_tasks_.clear();
  }
#endif
}

/**
 * @brief 检查当前线程是否属于该执行器
 * @return true 如果当前线程是执行器的工作线程之一
 * @return false 如果当前线程不属于该执行器
 * @details 通过比对当前线程ID是否在执行器的线程ID列表中来判断
 */
bool NativeThreadExecutor::IsInCurrentExecutor() const noexcept {
  try {
    std::thread::id current_id = std::this_thread::get_id();
    return std::find(thread_id_vec_.begin(), thread_id_vec_.end(), current_id) != thread_id_vec_.end();
  } catch (...) {
    return false;
  }
}

/**
 * @brief 执行一个异步任务
 * @param task 要执行的任务（右值引用）
 * @note 如果任务队列已满或执行器状态不正确，任务将被丢弃
 * @details 执行流程：
 *          1. 检查执行器状态
 *          2. 验证任务队列容量
 *          3. 必要时发出队列接近阈值警告
 *          4. 将任务添加到队列并通知工作线程
 */
void NativeThreadExecutor::Execute(aimrt::executor::Task&& task) noexcept {
  /**
   * @brief 检查执行器状态
   * @details 只有在Init或Start状态下才能执行任务
   */
  if (omnirt_unlikely(state_.load() != State::kInit && state_.load() != State::kStart)) {
    fprintf(stderr,
            "Native thread executor '%s' can only execute task when state is 'Init' or 'Start'.\n",
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
            "The number of tasks in the native thread executor '%s' has reached the threshold '%u', the task will not be delivered.\n",
            name_.c_str(), queue_threshold_);
    --queue_task_num_;
    return;
  }

  // 接近队列阈值，发出警告
  if (omnirt_unlikely(cur_queue_task_num > queue_warn_threshold_)) {
    fprintf(stderr,
            "The number of tasks in the native thread executor '%s' is about to reach the threshold: '%u / %u'.\n",
            name_.c_str(), cur_queue_task_num, queue_threshold_);
  }

  try {
    // 将任务添加到队列并通知一个工作线程
    std::lock_guard<std::mutex> lock(queue_mutex_);
    task_queue_.push_back(std::move(task));
    queue_condition_.notify_one();
  } catch (const std::exception& e) {
    fprintf(stderr, "Native thread executor '%s' execute Task get exception: %s\n", name_.c_str(), e.what());
    --queue_task_num_;
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
 *          2. 创建定时任务
 *          3. 添加到定时任务优先队列
 *          4. 通知定时器线程重新检查任务
 */
void NativeThreadExecutor::ExecuteAt(
    std::chrono::system_clock::time_point tp, aimrt::executor::Task&& task) noexcept {
  /**
   * @brief 检查执行器状态
   * @details 只有在Init或Start状态下才能执行任务
   */
  if (omnirt_unlikely(state_.load() != State::kInit && state_.load() != State::kStart)) {
    fprintf(stderr,
            "Native thread executor '%s' can only execute task when state is 'Init' or 'Start'.\n",
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
            "The number of tasks in the native thread executor '%s' has reached the threshold '%u', the task will not be delivered.\n",
            name_.c_str(), queue_threshold_);
    --queue_task_num_;
    return;
  }

  // 接近队列阈值，发出警告
  if (omnirt_unlikely(cur_queue_task_num > queue_warn_threshold_)) {
    fprintf(stderr,
            "The number of tasks in the native thread executor '%s' is about to reach the threshold: '%u / %u'.\n",
            name_.c_str(), cur_queue_task_num, queue_threshold_);
  }

#if 0
  try {
    // 创建定时任务并添加到定时任务队列
    std::lock_guard<std::mutex> lock(timer_mutex_);
    timer_tasks_.push_back({tp, std::move(task)});
    
    // 维护优先队列特性 - 按时间排序
    std::push_heap(timer_tasks_.begin(), timer_tasks_.end());
    
    // 标记定时器队列已更新
    timer_updated_ = true;
    
    // 通知定时器线程重新检查任务
    timer_condition_.notify_one();
  } catch (const std::exception& e) {
    fprintf(stderr, "Native thread executor '%s' ExecuteAt Task get exception: %s\n", name_.c_str(), e.what());
    --queue_task_num_;
  }
#endif
}

/**
 * @brief 工作线程函数，负责从任务队列中获取和执行任务
 */
void NativeThreadExecutor::WorkerThread(uint32_t i) {
  while (running_) {
    aimrt::executor::Task task;
    bool has_task = false;

    // 记录线程ID，用于后续判断任务是否在当前执行器中执行
    // 当线程开始运行时，使用std::this_thread::get_id()来记录实际的线程ID
    thread_id_vec_[i] = std::this_thread::get_id();

    // 设置线程名称，方便调试
    std::string threadname = name_;
    if (options_.thread_num > 1) {
      threadname = threadname + "." + std::to_string(i);
      util::SetNameForCurrentThread(threadname);
    }

    // 设置线程的CPU绑定和调度策略
    if (!options_.thread_bind_cpu.empty() && i < options_.thread_bind_cpu.size()) {
      if (!options_.thread_sched_policy.empty()) {
        util::SetCpuSchedForCurrentThread(options_.thread_sched_policy);
      }

      util::BindCpuForCurrentThread(options_.thread_bind_cpu);
    }

    // 从任务队列获取任务
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      
      // 等待任务或停止信号
      queue_condition_.wait(lock, [this] {
        return !task_queue_.empty() || !running_;
      });
      
      // 如果执行器已停止，退出线程
      if (!running_) {
        break;
      }
      
      // 存在任务，取出并执行
      if (!task_queue_.empty()) {
        task = std::move(task_queue_.front());
        task_queue_.pop_front();
        has_task = true;
      }
    }
    
    // 执行任务
    if (has_task) {
      try {
        task();
      } catch (const std::exception& e) {
        fprintf(stderr, "Native thread executor '%s' task execution exception: %s\n", name_.c_str(), e.what());
      } catch (...) {
        fprintf(stderr, "Native thread executor '%s' task execution unknown exception\n", name_.c_str());
      }
      
      // 递减任务计数
      --queue_task_num_;
    }
  }
}

/**
 * @brief 定时器处理线程，负责管理定时任务的执行
 */
#if 0
void NativeThreadExecutor::ProcessTimers() {
  while (running_) {
    std::chrono::system_clock::time_point next_time_point;
    bool has_task = false;
    
    // 查找最近需要执行的定时任务
    {
      std::unique_lock<std::mutex> lock(timer_mutex_);
      
      // 等待定时任务被添加或更新，或者执行器停止
      if (timer_tasks_.empty()) {
        timer_condition_.wait(lock, [this] {
          return timer_updated_ || !running_;
        });
        timer_updated_ = false;
      }
      
      // 如果执行器已停止，退出线程
      if (!running_) {
        break;
      }
      
      // 检查是否有可执行的定时任务
      if (!timer_tasks_.empty()) {
        // 优先队列的顶部是最早需要执行的任务
        next_time_point = timer_tasks_.front().time_point;
        has_task = true;
      }
    }
    
    if (has_task) {
      // 计算需要等待的时间
      auto now = std::chrono::system_clock::now();
      
      if (now >= next_time_point) {
        // 当前时间已经达到或超过执行时间，立即执行任务
        aimrt::executor::Task task;
        
        {
          std::lock_guard<std::mutex> lock(timer_mutex_);
          if (!timer_tasks_.empty() && timer_tasks_.front().time_point <= now) {
            // 获取任务并从队列中移除
            std::pop_heap(timer_tasks_.begin(), timer_tasks_.end());
            task = std::move(timer_tasks_.back().task);
            timer_tasks_.pop_back();
          }
        }
        
        if (task) {
          // 计算实际执行延迟时间
          auto dif_time = now - next_time_point;
          
          // 执行任务
          try {
            task();
          } catch (const std::exception& e) {
            fprintf(stderr, "Native thread executor '%s' timer task exception: %s\n", name_.c_str(), e.what());
          }
          
          // 检查执行延迟是否超过阈值
          if (dif_time > options_.timeout_alarm_threshold_us) {
            fprintf(stderr, 
                "Native thread executor '%s' timer delay too much, error time value '%ld us', require '%ld us'. "
                "Perhaps the CPU load is too high\n", 
                name_.c_str(), 
                std::chrono::duration_cast<std::chrono::microseconds>(dif_time).count(),
                std::chrono::duration_cast<std::chrono::microseconds>(options_.timeout_alarm_threshold_us).count());
          }
          
          // 递减任务计数
          --queue_task_num_;
        }
      } else {
        // 等待到达执行时间点或被新任务唤醒
        std::unique_lock<std::mutex> lock(timer_mutex_);
        timer_condition_.wait_until(lock, next_time_point, [this, &next_time_point] {
          return timer_updated_ || !running_ || (!timer_tasks_.empty() && timer_tasks_.front().time_point < next_time_point);
        });
        timer_updated_ = false;
      }
    }
  }
}
#endif

}  // namespace aimrt::runtime::core::executor