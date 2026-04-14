/**
 * @file test_timestamp.cpp
 * @brief Timestamp 单元测试
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "camera_toolkit/timestamp.h"

namespace {

constexpr int kWidth = 640;
constexpr int kHeight = 480;
// YUV420 Y 平面大小
constexpr int kYPlaneSize = kWidth * kHeight;

// 创建全零的 Y 平面缓冲区
std::vector<uint8_t> makeYPlane(uint8_t fill = 128) {
  return std::vector<uint8_t>(kYPlaneSize, fill);
}

}  // namespace

// ============================================================================
// 默认参数测试
// ============================================================================

TEST(TimestampTest, DefaultParams) {
  camera_toolkit::TimestampParams params;
  camera_toolkit::Timestamp ts(params);

  EXPECT_EQ(ts.getParams().startX, 10);
  EXPECT_EQ(ts.getParams().startY, 10);
  EXPECT_EQ(ts.getParams().videoWidth, 640);
  EXPECT_EQ(ts.getParams().factor, 0);
}

TEST(TimestampTest, CustomParams) {
  camera_toolkit::TimestampParams params;
  params.startX = 20;
  params.startY = 30;
  params.videoWidth = 1280;
  params.factor = 1;

  camera_toolkit::Timestamp ts(params);

  EXPECT_EQ(ts.getParams().startX, 20);
  EXPECT_EQ(ts.getParams().startY, 30);
  EXPECT_EQ(ts.getParams().videoWidth, 1280);
  EXPECT_EQ(ts.getParams().factor, 1);
}

// ============================================================================
// draw() 绘制有效性测试
// ============================================================================

TEST(TimestampTest, DrawModifiesYPlane) {
  camera_toolkit::TimestampParams params;
  params.videoWidth = kWidth;
  camera_toolkit::Timestamp ts(params);

  auto yPlane = makeYPlane(128);
  ts.draw(yPlane.data());

  // draw() 完成后图像不应全为128（时间戳像素写入了非128值）
  bool anyModified = false;
  for (uint8_t v : yPlane) {
    if (v != 128) {
      anyModified = true;
      break;
    }
  }
  EXPECT_TRUE(anyModified);
}

TEST(TimestampTest, DrawDoesNotWriteOutsideYPlane) {
  camera_toolkit::TimestampParams params;
  params.videoWidth = kWidth;
  camera_toolkit::Timestamp ts(params);

  // 在 Y 平面后面添加 sentinel 字节
  std::vector<uint8_t> buffer(kYPlaneSize + 64, 0xAB);
  ts.draw(buffer.data());

  // sentinel 区域不应被修改
  for (int i = kYPlaneSize; i < kYPlaneSize + 64; ++i) {
    EXPECT_EQ(buffer[i], 0xAB) << "Sentinel byte at offset " << i << " was overwritten";
  }
}

// ============================================================================
// drawText() 测试
// ============================================================================

TEST(TimestampTest, DrawTextModifiesYPlane) {
  camera_toolkit::TimestampParams params;
  params.videoWidth = kWidth;
  camera_toolkit::Timestamp ts(params);

  auto yPlane = makeYPlane(128);
  ts.drawText(yPlane.data(), "HELLO");

  bool anyModified = false;
  for (uint8_t v : yPlane) {
    if (v != 128) {
      anyModified = true;
      break;
    }
  }
  EXPECT_TRUE(anyModified);
}

TEST(TimestampTest, DrawTextEmptyStringDoesNotCrash) {
  camera_toolkit::TimestampParams params;
  params.videoWidth = kWidth;
  camera_toolkit::Timestamp ts(params);

  auto yPlane = makeYPlane(128);
  // 空字符串不应崩溃
  EXPECT_NO_THROW(ts.drawText(yPlane.data(), ""));
}

TEST(TimestampTest, DrawTextNullStringDoesNotCrash) {
  camera_toolkit::TimestampParams params;
  params.videoWidth = kWidth;
  camera_toolkit::Timestamp ts(params);

  auto yPlane = makeYPlane(128);
  // nullptr 不应崩溃
  EXPECT_NO_THROW(ts.drawText(yPlane.data(), nullptr));
}

// ============================================================================
// 大字体测试
// ============================================================================

TEST(TimestampTest, LargeFontDraw) {
  camera_toolkit::TimestampParams params;
  params.videoWidth = kWidth;
  params.factor = 1;  // 大字体
  camera_toolkit::Timestamp ts(params);

  auto yPlane = makeYPlane(128);
  ts.draw(yPlane.data());

  bool anyModified = false;
  for (uint8_t v : yPlane) {
    if (v != 128) {
      anyModified = true;
      break;
    }
  }
  EXPECT_TRUE(anyModified);
}

// ============================================================================
// 多次绘制一致性测试
// ============================================================================

TEST(TimestampTest, DrawTextSameContentProducesSameResult) {
  camera_toolkit::TimestampParams params;
  params.videoWidth = kWidth;
  camera_toolkit::Timestamp ts(params);

  auto yPlane1 = makeYPlane(128);
  auto yPlane2 = makeYPlane(128);

  ts.drawText(yPlane1.data(), "TEST");
  ts.drawText(yPlane2.data(), "TEST");

  EXPECT_EQ(yPlane1, yPlane2);
}
