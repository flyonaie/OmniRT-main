#include <thread>
#include <csignal>
#include <iostream>
#include "util/block_queue.h"

// #include "aimrt_module_cpp_interface/core.h"

using namespace aimrt::common::util;

void blocktest() {
  BlockQueue<int> queue;
  std::vector<std::thread> threads;

  threads.emplace_back([&]() {
    for (int i = 0; i < 5; ++i) {
      queue.Enqueue(i);
      std::cout << "Enqueue " << i << std::endl;
    }
  });

  threads.emplace_back([&]() {
    for (int i = 0; i < 5; ++i) {
      auto item = queue.Dequeue();
      std::cout << "Enqueue item" << item << std::endl;
    }
  });

  for (auto& t : threads) {
    t.join();
  }
}

int32_t main(int32_t argc, char** argv) {
 
  std::cout << "AimRT start." << std::endl;
  blocktest();

  std::cout << "AimRT exit." << std::endl;

  return 0;
}
