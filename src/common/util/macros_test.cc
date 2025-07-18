#include "common/util/macros.h"

#include <chrono>
#include <random>
#include <vector>

#include "gtest/gtest.h"
#include "util/macros.h"

namespace omnirt {
namespace common {
namespace util {
namespace {

// Helper function to measure execution time
template <typename Func>
double MeasureTimeMilliseconds(Func func, int iterations) {
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    func();
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  return duration.count() / 1000.0;  // Convert to milliseconds
}

// Test function with likely branch
int TestLikelyBranch(const std::vector<int>& values) {
  int sum = 0;
  for (int value : values) {
    if (omnirt_likely(value > 0)) {
      sum += value;
    }
  }
  return sum;
}

// Test function with unlikely branch
int TestUnlikelyBranch(const std::vector<int>& values) {
  int sum = 0;
  for (int value : values) {
    if (omnirt_unlikely(value < 0)) {
      sum += value;
    }
  }
  return sum;
}

// Test function without branch prediction
int TestNormalBranch(const std::vector<int>& values) {
  int sum = 0;
  for (int value : values) {
    if (value > 0) {
      sum += value;
    }
  }
  return sum;
}

TEST(MacrosTest, LikelyBranchPerformance) {
  constexpr int kSize = 1000000;
  constexpr int kIterations = 100;
  
  // Generate test data with mostly positive numbers (90% positive)
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(0, 1);
  
  std::vector<int> values;
  values.reserve(kSize);
  for (int i = 0; i < kSize; ++i) {
    values.push_back(dis(gen) < 0.9 ? 1 : -1);
  }

  // Measure time with likely branch
  double likely_time = MeasureTimeMilliseconds([&]() {
    TestLikelyBranch(values);
  }, kIterations);

  // Measure time with normal branch
  double normal_time = MeasureTimeMilliseconds([&]() {
    TestNormalBranch(values);
  }, kIterations);

  // Print results
  std::cout << "Likely branch time: " << likely_time << "ms" << std::endl;
  std::cout << "Normal branch time: " << normal_time << "ms" << std::endl;

  // Verify correctness
  EXPECT_EQ(TestLikelyBranch(values), TestNormalBranch(values));
}

TEST(MacrosTest, UnlikelyBranchPerformance) {
  constexpr int kSize = 1000000;
  constexpr int kIterations = 100;
  
  // Generate test data with mostly positive numbers (90% positive)
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(0, 1);
  
  std::vector<int> values;
  values.reserve(kSize);
  for (int i = 0; i < kSize; ++i) {
    values.push_back(dis(gen) < 0.9 ? 1 : -1);
  }

  // Test function with unlikely branch
  auto unlikely_result = TestUnlikelyBranch(values);
  
  // Calculate expected result
  int expected_result = 0;
  for (int value : values) {
    if (value < 0) {
      expected_result += value;
    }
  }

  // Verify correctness
  EXPECT_EQ(unlikely_result, expected_result);
}

TEST(MacrosTest, BranchPredictionBasicTest) {
  // Test basic functionality of likely/unlikely macros
  bool condition = true;
  
  EXPECT_TRUE(omnirt_likely(condition));
  EXPECT_FALSE(omnirt_likely(false));
  
  EXPECT_TRUE(omnirt_unlikely(condition));
  EXPECT_FALSE(omnirt_unlikely(false));
}

}  // namespace
}  // namespace util
}  // namespace common
}  // namespace omnirt
