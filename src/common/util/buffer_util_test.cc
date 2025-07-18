// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.
//
// 缓冲区工具类的单元测试
// 测试内容包括:
// 1. 基本数据类型的序列化和反序列化
// 2. 字符串的序列化和反序列化
// 3. 缓冲区操作类的功能验证
// 4. 边界条件和异常情况的处理

#include <gtest/gtest.h>

#include "util/buffer_util.h"

namespace aimrt::common::util {

/**
 * @brief 测试基本数据类型的序列化和反序列化
 * 
 * 测试SetBufFromUintXX和GetUintXXFromBuf函数:
 * - uint16_t类型(2字节)
 * - uint32_t类型(4字节)
 * - uint64_t类型(8字节)
 */
TEST(SetGetBufTest, base) {
  char buf[8];

  const uint16_t n16 = 0x0123;
  SetBufFromUint16(buf, n16);
  EXPECT_EQ(GetUint16FromBuf(buf), n16);

  const uint32_t n32 = 0x01234567;
  SetBufFromUint32(buf, n32);
  EXPECT_EQ(GetUint32FromBuf(buf), n32);

  const uint64_t n64 = 0x0123456789ABCDEF;
  SetBufFromUint64(buf, n64);
  EXPECT_EQ(GetUint64FromBuf(buf), n64);
}

/**
 * @brief 测试缓冲区操作类的基本功能
 * 
 * 测试BufferOperator和ConstBufferOperator的:
 * - 基本数据类型的读写
 * - 字符串的读写(不同长度类型)
 * - 缓冲区的读写
 * - 剩余空间计算
 */
TEST(BufferOperatorTest, base) {
  char buf[1024];

  uint8_t n8 = 0x01;
  uint16_t n16 = 0x0123;
  uint32_t n32 = 0x01234567;
  uint64_t n64 = 0x0123456789ABCDEF;
  std::string_view s("0123456789ABCDEF");

  BufferOperator op(buf, sizeof(buf));

  op.SetUint8(n8);
  op.SetString(s, BufferLenType::kUInt8);
  op.SetUint16(n16);
  op.SetString(s, BufferLenType::kUInt16);
  op.SetUint32(n32);
  op.SetString(s, BufferLenType::kUInt32);
  op.SetUint64(n64);
  op.SetString(s, BufferLenType::kUInt64);
#ifdef SPAN_FEATURE
  op.SetBuffer(gsl::span<const char>(s.data(), s.size()));
#else
  op.SetBuffer(s.data(), s.size());
#endif

  ConstBufferOperator cop(buf, sizeof(buf));
  ASSERT_EQ(cop.GetUint8(), n8);
  ASSERT_EQ(cop.GetString(BufferLenType::kUInt8), s);
  ASSERT_EQ(cop.GetUint16(), n16);
  ASSERT_EQ(cop.GetString(BufferLenType::kUInt16), s);
  ASSERT_EQ(cop.GetUint32(), n32);
  ASSERT_EQ(cop.GetString(BufferLenType::kUInt32), s);
  ASSERT_EQ(cop.GetUint64(), n64);
  ASSERT_EQ(cop.GetString(BufferLenType::kUInt64), s);

  auto buf_span = cop.GetBuffer(s.size());
  // ASSERT_EQ(std::string_view(buf_span.data(), buf_span.size()), s);
  ASSERT_EQ(std::string_view(buf_span, s.size()), s);

  ASSERT_EQ(op.GetRemainingSize(), cop.GetRemainingSize());
}

}  // namespace aimrt::common::util