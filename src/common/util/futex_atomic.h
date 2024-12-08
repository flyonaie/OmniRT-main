#pragma once

#include <atomic>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace aimrt::common::util {

inline long futex(uint32_t* uaddr, int op, uint32_t val,
                 const struct timespec* timeout = nullptr,
                 uint32_t* uaddr2 = nullptr, uint32_t val3 = 0) {
  return syscall(SYS_futex, uaddr, op, val, timeout, uaddr2, val3);
}

class FutexAtomic {
 public:
  FutexAtomic() noexcept = default;
  ~FutexAtomic() = default;

  void wait(bool old) const noexcept {
    // 首先进行快速检查和自旋
    static constexpr int SPIN_COUNT = 4000;
    for (int i = 0; i < SPIN_COUNT; ++i) {
      if (flag_.load(std::memory_order_acquire) != old) {
        return;
      }
      asm volatile("pause" ::: "memory");  // Intel PAUSE 指令
    }

    // 如果自旋后仍未满足条件，使用 futex
    while (flag_.load(std::memory_order_acquire) == old) {
      futex(reinterpret_cast<uint32_t*>(const_cast<std::atomic<bool>*>(&flag_)),
            FUTEX_WAIT_PRIVATE, old ? 1 : 0);
    }
  }

  void notify_one() noexcept {
    futex(reinterpret_cast<uint32_t*>(&flag_), FUTEX_WAKE_PRIVATE, 1);
  }

  void notify_all() noexcept {
    futex(reinterpret_cast<uint32_t*>(&flag_), FUTEX_WAKE_PRIVATE, INT_MAX);
  }

  void store(bool desired, std::memory_order order = std::memory_order_seq_cst) noexcept {
    flag_.store(desired, order);
  }

  bool load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
    return flag_.load(order);
  }

  operator bool() const noexcept {
    return load();
  }

  FutexAtomic& operator=(bool desired) noexcept {
    store(desired);
    return *this;
  }

 private:
  alignas(4) std::atomic<bool> flag_{false};
};

#if __cplusplus >= 202002L
#include <version>
#endif

}  // namespace aimrt::common::util
