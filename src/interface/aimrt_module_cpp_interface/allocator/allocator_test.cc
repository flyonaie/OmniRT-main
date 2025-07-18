#include "allocator/allocator.h"
#include "gtest/gtest.h"

namespace aimrt::allocator {
namespace {

// Mock allocator implementation
void* MockGetThreadLocalBuf(void* impl, size_t buf_size) {
  static char buffer[1024];  // Static buffer for testing
  return (buf_size <= sizeof(buffer)) ? buffer : nullptr;
}

class AllocatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_allocator_.get_thread_local_buf = MockGetThreadLocalBuf;
    mock_allocator_.impl = nullptr;  // Not used in our mock
  }

  aimrt_allocator_base_t mock_allocator_;
};

// Test constructor and bool operator
TEST_F(AllocatorTest, ConstructorAndBoolOperator) {
  // Test default constructor
  AllocatorRef empty_ref;
  EXPECT_FALSE(empty_ref);
  EXPECT_EQ(empty_ref.NativeHandle(), nullptr);

  // Test constructor with valid allocator
  AllocatorRef valid_ref(&mock_allocator_);
  EXPECT_TRUE(valid_ref);
  EXPECT_EQ(valid_ref.NativeHandle(), &mock_allocator_);
}

// Test GetThreadLocalBuf functionality
TEST_F(AllocatorTest, GetThreadLocalBuf) {
  AllocatorRef allocator(&mock_allocator_);
  
  // Test valid buffer size request
  void* buf = allocator.GetThreadLocalBuf(512);  // Should succeed
  EXPECT_NE(buf, nullptr);

  // Test oversized buffer request
  void* large_buf = allocator.GetThreadLocalBuf(2048);  // Should fail
  EXPECT_EQ(large_buf, nullptr);
}

}  // namespace
}  // namespace aimrt::allocator
