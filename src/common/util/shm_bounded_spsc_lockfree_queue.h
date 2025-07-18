// Copyright (c) 2023 OmniRT Authors. All rights reserved.
//
// 基于共享内存的有界单生产者单消费者无锁队列实现
// 本队列实现专门针对需要跨进程通信的单生产者-单消费者场景进行了优化,具有以下特点:
// 1. 无锁实现: 通过原子操作和内存序保证线程安全,避免锁开销
// 2. 有界队列: 预分配固定大小的存储空间,避免动态内存分配
// 3. SPSC特化: 仅支持单生产者-单消费者模式,但性能最优
// 4. 缓存友好: 通过字节对齐减少伪共享,提升缓存命中率
// 5. 共享内存: 支持跨进程通信,可用于不同进程间的数据交换
//
// 使用场景:
// - 高性能进程间通信,如生产者-消费者模型
// - 实时系统中的跨进程数据传输
// - 对延迟敏感的分布式应用程序
//
// 线程安全说明:
// - 一个进程的一个线程可以安全地执行入队操作(生产者)
// - 另一个进程的一个线程可以安全地执行出队操作(消费者)
// - 同一个进程/线程不能同时执行入队和出队操作

#pragma once

#include <atomic>
#include <cstddef>  // for size_t
#include <cstdint>
#include <cstdlib>  // for std::free
#include <sys/mman.h>  // 共享内存相关
#include <sys/stat.h>  // 共享内存相关
#include <fcntl.h>     // 共享内存相关
#include <unistd.h>    // for close, ftruncate
#include <string>
#include <utility>
#include <memory>
#include <new>         // for placement new
#include <iostream>    // for std::cout, std::cerr
#include <cstring>     // for strerror

#include "bounded_spsc_lockfree_queue.h"  // 基类头文件

namespace omnirt::common::util {

/**
 * @brief 基于共享内存的有界单生产者单消费者无锁队列
 * 
 * 这是一个高性能的进程间通信队列,专门为单生产者-单消费者场景优化。
 * 队列使用无锁设计和共享内存机制,通过原子操作和内存序(memory ordering)保证进程间安全。
 * 继承自BoundedSpscLockfreeQueue基类,扩展了共享内存支持。
 * 
 * 主要特性:
 * 1. 预分配共享内存空间,避免运行时内存分配
 * 2. 使用原子操作代替互斥锁,降低同步开销
 * 3. 通过内存对齐优化缓存访问,减少伪共享
 * 4. 支持普通入队/出队和覆盖式入队/最新出队等操作
 * 5. 支持跨进程通信,可用于不同进程间的数据交换
 * 
 * 注意事项:
 * - 队列大小在初始化时指定,之后不可更改
 * - 当指定2的幂大小时,索引计算会使用位运算优化
 * - 生产者和消费者必须是不同的进程或线程
 * - 使用T类型必须支持在共享内存中正确构造和析构
 * 
 * @tparam T 队列中存储的元素类型,必须是POD类型或者确保在共享内存中安全使用
 */
template <typename T>
class ShmBoundedSpscLockfreeQueue : public BoundedSpscLockfreeQueue<T> {
 public:
  // 使用基类定义的常量
  using typename BoundedSpscLockfreeQueue<T>::QueueHeader;
  using BoundedSpscLockfreeQueue<T>::QUEUE_MAX_SIZE;
  using BoundedSpscLockfreeQueue<T>::CACHELINE_SIZE;
  
  // 共享内存状态枚举
  enum class ShmState {
    NOT_INITIALIZED,  ///< 未初始化
    CREATED,          ///< 创建者(第一个打开共享内存的进程)
    ATTACHED          ///< 附加者(后续打开共享内存的进程)
  };

  // 构造和析构
  ShmBoundedSpscLockfreeQueue();
  ~ShmBoundedSpscLockfreeQueue();
  
  // 禁用拷贝和移动
  ShmBoundedSpscLockfreeQueue(const ShmBoundedSpscLockfreeQueue&) = delete;
  ShmBoundedSpscLockfreeQueue& operator=(const ShmBoundedSpscLockfreeQueue&) = delete;
  ShmBoundedSpscLockfreeQueue(ShmBoundedSpscLockfreeQueue&&) = delete;
  ShmBoundedSpscLockfreeQueue& operator=(ShmBoundedSpscLockfreeQueue&&) = delete;

  /**
   * @brief 初始化队列
   * 
   * 创建或打开一个共享内存区域,并初始化队列。如果共享内存区域不存在,则创建;
   * 如果已存在,则附加到现有的共享内存区域。
   * 
   * @param shm_name 共享内存名称,以/开头的唯一标识符,例如"/my_queue"
   * @param size 队列大小,必须大于0且不超过QUEUE_MAX_SIZE
   * @param force_power_of_two 是否强制size为2的幂
   * @param creator 是否作为创建者打开共享内存(true=创建/false=附加)
   * @return true 初始化成功
   * @return false 初始化失败(参数无效、内存分配失败或共享内存访问错误)
   */
  bool Init(const std::string& shm_name, uint64_t size, bool force_power_of_two = false, bool creator = true);

  /**
   * @brief 将元素入队
   * 
   * 尝试将元素添加到队列尾部。如果队列已满,则入队失败。
   * 该方法线程安全且可跨进程,但只能由生产者进程/线程调用。
   * 
   * @param element 要入队的元素
   * @return true 入队成功
   * @return false 队列已满或未初始化
   */
  bool Enqueue(const T& element) { return EnqueueInternal(element, false); }
  bool Enqueue(T&& element) { return EnqueueInternal(std::move(element), false); }
  
  /**
   * @brief 强制将元素入队
   * 
   * 将元素添加到队列尾部。如果队列已满,则覆盖最旧的元素。
   * 该方法线程安全且可跨进程,但只能由生产者进程/线程调用。
   * 
   * @param element 要入队的元素
   * @return true 入队成功(除非队列未初始化,否则总是成功)
   * @return false 队列未初始化
   */
  bool EnqueueOverwrite(const T& element) { return EnqueueInternal(element, true); }
  bool EnqueueOverwrite(T&& element) { return EnqueueInternal(std::move(element), true); }

  /**
   * @brief 从队列中取出最早的元素
   * 
   * 尝试从队列头部取出元素。如果队列为空,则出队失败。
   * 该方法线程安全且可跨进程,但只能由消费者进程/线程调用。
   * 
   * @param[out] element 用于存储出队元素的指针
   * @return true 出队成功
   * @return false 队列为空或未初始化或element为空
   */
  bool Dequeue(T* element);

  /**
   * @brief 从队列中取出最新的元素
   * 
   * 尝试获取队列中最新的元素(tail-1位置),并丢弃所有更早的元素。
   * 该方法线程安全且可跨进程,但只能由消费者进程/线程调用。
   * 
   * @param[out] element 用于存储出队元素的指针
   * @return true 出队成功
   * @return false 队列为空或未初始化或element为空
   */
  bool DequeueLatest(T* element);

  // 状态查询
  /**
   * @brief 返回队列当前元素数量
   * 
   * 获取队列中当前的元素数量。
   * 
   * @return 当前队列大小
   */
  uint64_t Size() const;
  
  /**
   * @brief 检查队列是否为空
   * 
   * 判断队列是否为空。
   * 
   * @return true 队列为空
   * @return false 队列不为空
   */
  bool Empty() const { return Size() == 0; }
  
  /**
   * @brief 返回队列最大容量
   * 
   * 获取队列的最大容量。
   * 
   * @return 队列容量
   */
  uint64_t Capacity() const { return pool_size_; }

  /**
   * @brief 获取共享内存状态
   * 
   * 返回当前共享内存的状态(未初始化/创建者/附加者)。
   * 
   * @return 共享内存状态
   */
  ShmState GetShmState() const { return shm_state_; }

  /**
   * @brief 关闭并解除映射共享内存
   * 
   * 关闭共享内存连接并释放相关资源。如果是创建者,还会尝试删除共享内存对象。
   */
  void Close();
  
 protected:
  // 受保护的辅助方法
  bool EnqueueInternal(const T& element, bool overwrite);

 private:
  /**
   * @brief 检查一个数是否是2的幂
   * 
   * 判断一个数是否是2的幂。
   * 
   * @param x 要检查的数
   * @return true x是2的幂
   * @return false x不是2的幂
   */
  static bool IsPowerOfTwo(uint64_t x) { return x > 0 && (x & (x - 1)) == 0; }
  
  /**
   * @brief 计算循环缓冲区中的索引
   * 
   * 根据逻辑位置计算实际的缓冲区索引。
   * 
   * @param num 逻辑位置
   * @return 实际缓冲区索引
   */
  uint64_t GetIndex(uint64_t num) const {
    // 2的幂情况：位掩码(~0.5ns),非2的幂情况：使用减法方法(~12ns)
    return header_->use_mask_ ? (num & header_->pool_size_mask_) 
                            : (num - (num / header_->pool_size_) * header_->pool_size_);
  }

  /**
   * @brief 计算共享内存需要的总大小
   * 
   * 计算包含队列头部和元素存储区域所需的共享内存总大小。
   * 
   * @param queue_size 队列容量(元素个数)
   * @return 所需的共享内存总大小(字节)
   */
  static size_t CalculateTotalSize(uint64_t queue_size) {
    return sizeof(QueueHeader) + queue_size * sizeof(T);
  }

  // 使用基类的QueueHeader结构体

  // 成员变量
  using BoundedSpscLockfreeQueue<T>::header_;
  using BoundedSpscLockfreeQueue<T>::pool_;
  uint64_t pool_size_{0};            // 队列容量(本地缓存)
  
  int shm_fd_{-1};                   // 共享内存文件描述符
  std::string shm_name_;             // 共享内存名称
  size_t shm_size_{0};               // 共享内存大小
  ShmState shm_state_{ShmState::NOT_INITIALIZED};  // 共享内存状态
  void* shm_addr_{nullptr};          // 共享内存映射的起始地址
  bool is_creator_{false};           // 是否是创建者
};

template <typename T>
ShmBoundedSpscLockfreeQueue<T>::ShmBoundedSpscLockfreeQueue() : BoundedSpscLockfreeQueue<T>(),
  pool_size_(0), shm_fd_(-1), shm_size_(0), shm_addr_(nullptr), shm_state_(ShmState::NOT_INITIALIZED), is_creator_(false) {}

template <typename T>
ShmBoundedSpscLockfreeQueue<T>::~ShmBoundedSpscLockfreeQueue() {
  Close();
}


template <typename T>
bool ShmBoundedSpscLockfreeQueue<T>::Init(const std::string& shm_name, uint64_t size, 
                                         bool force_power_of_two, bool creator) {
  std::cout << "DEBUG: Initializing shared memory: " << shm_name << ", size: " << size 
            << ", creator: " << (creator ? "true" : "false") << std::endl;
  if (shm_state_ != ShmState::NOT_INITIALIZED || size == 0 || size > QUEUE_MAX_SIZE) {
    std::cerr << "DEBUG: 无效的初始化状态或大小, shm_state_=" << static_cast<int>(shm_state_) 
              << ", size=" << size << ", QUEUE_MAX_SIZE=" << QUEUE_MAX_SIZE << std::endl;
    return false;
  }

  if (shm_name.empty() || shm_name[0] != '/') {
    // 共享内存名称必须以/开头
    std::cerr << "Invalid shared memory name, must start with '/': " << shm_name << std::endl;
    return false;
  }
  
  std::cout << "DEBUG: Initializing shared memory: " << shm_name << ", size: " << size 
            << ", creator: " << (creator ? "true" : "false") << std::endl;

  if (force_power_of_two && !IsPowerOfTwo(size)) {
    // 如果要求2的幂但size不是2的幂，直接返回错误
    std::cerr << "DEBUG: 非2的幂大小, size=" << size << ", force_power_of_two=" << (force_power_of_two ? "true" : "false") << std::endl;
    return false;
  }

  shm_name_ = shm_name;
  pool_size_ = size;
  shm_size_ = CalculateTotalSize(size);
  
  std::cout << "DEBUG: 内存计算: pool_size_=" << pool_size_ 
            << ", 元素大小=" << sizeof(T) 
            << ", 总大小=" << shm_size_ << std::endl;

  // 创建或打开共享内存
  int flags = O_RDWR;
  if (creator) {
    flags |= O_CREAT | O_EXCL;  // 创建者模式：如果存在则失败
    std::cout << "DEBUG: 尝试创建共享内存，标志：" << flags << ", 权限: 0666" << std::endl;
    std::cout << "DEBUG: 共享内存名称：" << shm_name_.c_str() << std::endl;
    shm_fd_ = shm_open(shm_name_.c_str(), flags, 0666);
    if (shm_fd_ == -1) {
      if (errno == EEXIST) {
        // 共享内存已存在，切换到附加模式
        flags = O_RDWR;
        shm_fd_ = shm_open(shm_name_.c_str(), flags, 0666);
        if (shm_fd_ == -1) {
          std::cerr << "Failed to attach to existing shared memory: " << strerror(errno) << std::endl;
          return false;
        }
        shm_state_ = ShmState::ATTACHED;
      } else {
        std::cerr << "Failed to create shared memory: " << strerror(errno) << " (errno=" << errno << ")" << std::endl;
        perror("shm_open");
        std::cout << "DEBUG: 创建共享内存失败，退出" << std::endl;
        return false;
      }
    } else {
      std::cout << "DEBUG: 成功创建共享内存, fd=" << shm_fd_ << std::endl;
      // 成功创建，设置大小
      std::cout << "DEBUG: 尝试设置共享内存大小: " << shm_size_ << std::endl;
      if (ftruncate(shm_fd_, shm_size_) == -1) {
        std::cerr << "Failed to set shared memory size: " << strerror(errno) << " (errno=" << errno << ")" << std::endl;
        close(shm_fd_);
        shm_unlink(shm_name_.c_str());
        shm_fd_ = -1;
        return false;
      }
      shm_state_ = ShmState::CREATED;
    }
  } else {
    // 附加者模式：只打开已存在的共享内存
    shm_fd_ = shm_open(shm_name_.c_str(), flags, 0666);
    if (shm_fd_ == -1) {
      std::cerr << "Failed to open shared memory: " << strerror(errno) << " (errno=" << errno << ")" << std::endl;
      return false;
    }
    shm_state_ = ShmState::ATTACHED;
  }

  // 映射共享内存
  std::cout << "DEBUG: Mapping shared memory, size: " << shm_size_ << ", fd: " << shm_fd_ << std::endl;
  shm_addr_ = mmap(nullptr, shm_size_, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
  if (shm_addr_ == MAP_FAILED) {
    std::cerr << "Failed to map shared memory: " << strerror(errno) << " (errno=" << errno << ")" << std::endl;
    perror("mmap");
    std::cout << "DEBUG: MAP_FAILED 映射失败，检查共享内存大小和权限" << std::endl;
    
    // 检查共享内存文件的状态
    struct stat sb;
    if (fstat(shm_fd_, &sb) == 0) {
      std::cout << "DEBUG: 共享内存文件大小 = " << sb.st_size << " 字节" << std::endl;
    } else {
      std::cerr << "DEBUG: 无法获取共享内存文件信息: " << strerror(errno) << std::endl;
    }
    
    close(shm_fd_);
    if (shm_state_ == ShmState::CREATED) {
      shm_unlink(shm_name_.c_str());
    }
    shm_fd_ = -1;
    shm_addr_ = nullptr;
    shm_state_ = ShmState::NOT_INITIALIZED;
    return false;
  }

  // 设置指针
  std::cout << "DEBUG: Successfully mapped shared memory at: " << shm_addr_ << std::endl;
  header_ = static_cast<QueueHeader*>(shm_addr_);
  pool_ = reinterpret_cast<T*>(static_cast<char*>(shm_addr_) + sizeof(QueueHeader));
  std::cout << "DEBUG: Header at: " << header_ << ", Pool at: " << (void*)pool_ << std::endl;

  // 如果是创建者，初始化队列结构和元素
  if (shm_state_ == ShmState::CREATED) {
    std::cout << "DEBUG: Initializing queue as creator" << std::endl;
    header_->pool_size_ = pool_size_;
    header_->use_mask_ = force_power_of_two || IsPowerOfTwo(size);
    header_->pool_size_mask_ = pool_size_ - 1;
    header_->head_.store(0, std::memory_order_relaxed);
    header_->tail_.store(0, std::memory_order_relaxed);
    std::cout << "DEBUG: Initialized header: pool_size_=" << header_->pool_size_ 
              << ", use_mask_=" << header_->use_mask_ 
              << ", pool_size_mask_=" << header_->pool_size_mask_ << std::endl;

    // 使用placement new初始化元素
    for (uint64_t i = 0; i < pool_size_; ++i) {
      new (&pool_[i]) T();
    }
  } else {
    // 附加者：验证元素大小匹配
    std::cout << "DEBUG: Validating queue as attacher" << std::endl;
    std::cout << "DEBUG: Header found: pool_size_=" << header_->pool_size_ 
              << ", use_mask_=" << header_->use_mask_ 
              << ", pool_size_mask_=" << header_->pool_size_mask_ << std::endl;
    if (header_->pool_size_ != pool_size_) {
      std::cerr << "Pool size mismatch in shared memory: expected " << pool_size_
                << ", got " << header_->pool_size_ << std::endl;
      Close();
      return false;
    }
  }

  return true;
}

template <typename T>
void ShmBoundedSpscLockfreeQueue<T>::Close() {
  if (shm_addr_ != nullptr && shm_addr_ != MAP_FAILED) {
    // 如果是创建者，调用析构函数清理元素
    if (shm_state_ == ShmState::CREATED && pool_ != nullptr) {
      for (uint64_t i = 0; i < pool_size_; ++i) {
        pool_[i].~T();
      }
    }

    // 解除映射
    munmap(shm_addr_, shm_size_);
    shm_addr_ = nullptr;
    header_ = nullptr;
    pool_ = nullptr;
  }

  // 关闭文件描述符
  if (shm_fd_ != -1) {
    close(shm_fd_);
    shm_fd_ = -1;
  }

  // 如果是创建者，删除共享内存对象
  if (shm_state_ == ShmState::CREATED && !shm_name_.empty()) {
    shm_unlink(shm_name_.c_str());
  }

  shm_state_ = ShmState::NOT_INITIALIZED;
}

template <typename T>
bool ShmBoundedSpscLockfreeQueue<T>::EnqueueInternal(const T& element, bool overwrite) {
  // 复用基类的方法
  return BoundedSpscLockfreeQueue<T>::EnqueueInternal(element, overwrite);
}

template <typename T>
bool ShmBoundedSpscLockfreeQueue<T>::Dequeue(T* element) {
  // 复用基类的方法
  return BoundedSpscLockfreeQueue<T>::Dequeue(element);
}

template <typename T>
bool ShmBoundedSpscLockfreeQueue<T>::DequeueLatest(T* element) {
  // 复用基类的方法
  return BoundedSpscLockfreeQueue<T>::DequeueLatest(element);
}

template <typename T>
uint64_t ShmBoundedSpscLockfreeQueue<T>::Size() const {
  // 复用基类的方法
  return BoundedSpscLockfreeQueue<T>::Size();
}

}  // namespace omnirt::common::util

