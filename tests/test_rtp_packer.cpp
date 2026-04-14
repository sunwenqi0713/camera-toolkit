/**
 * @file test_rtp_packer.cpp
 * @brief RTPPacker 单元测试
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "camera_toolkit/rtp_packer.h"

namespace {

// 构造带4字节起始码的 H264 NAL 数据
std::vector<uint8_t> makeNalu(uint8_t naluType, int payloadSize) {
  std::vector<uint8_t> data;
  // 4字节起始码
  data.push_back(0x00);
  data.push_back(0x00);
  data.push_back(0x00);
  data.push_back(0x01);
  // NALU 头：forbidden=0, nal_ref_idc=3, nal_unit_type
  data.push_back(static_cast<uint8_t>((3 << 5) | (naluType & 0x1f)));
  // 填充负载
  for (int i = 0; i < payloadSize; ++i) {
    data.push_back(static_cast<uint8_t>(i & 0xff));
  }
  return data;
}

// 从 RTP 包中读取 marker bit（字节1的最高位）
bool getRTPMarker(const camera_toolkit::Buffer& pkt) {
  const uint8_t* p = static_cast<const uint8_t*>(pkt.data);
  return (p[1] & 0x80) != 0;
}

// 从 RTP 包中读取 payload type（字节1低7位）
int getRTPPayloadType(const camera_toolkit::Buffer& pkt) {
  const uint8_t* p = static_cast<const uint8_t*>(pkt.data);
  return p[1] & 0x7f;
}

// 从 RTP 包中读取 sequence number（字节2-3，大端）
uint16_t getRTPSeqNo(const camera_toolkit::Buffer& pkt) {
  const uint8_t* p = static_cast<const uint8_t*>(pkt.data);
  return static_cast<uint16_t>((p[2] << 8) | p[3]);
}

}  // namespace

// ============================================================================
// 基础配置测试
// ============================================================================

TEST(RTPPackerTest, DefaultParams) {
  camera_toolkit::RTPPackerParams params;
  camera_toolkit::RTPPacker packer(params);

  EXPECT_EQ(packer.getParams().maxPacketLength, 1400);
  EXPECT_EQ(packer.getParams().ssrc, 1234u);
}

TEST(RTPPackerTest, CustomParams) {
  camera_toolkit::RTPPackerParams params;
  params.maxPacketLength = 512;
  params.ssrc = 0xDEADBEEF;

  camera_toolkit::RTPPacker packer(params);
  EXPECT_EQ(packer.getParams().maxPacketLength, 512);
  EXPECT_EQ(packer.getParams().ssrc, 0xDEADBEEFu);
}

// ============================================================================
// 空输入测试
// ============================================================================

TEST(RTPPackerTest, GetWithoutPutReturnsNullopt) {
  camera_toolkit::RTPPackerParams params;
  camera_toolkit::RTPPacker packer(params);

  // 未调用 put()，get() 应返回 nullopt
  auto result = packer.get();
  EXPECT_FALSE(result.has_value());
}

// ============================================================================
// 单 NAL 单元测试（小于 maxPacketLength）
// ============================================================================

TEST(RTPPackerTest, SingleSmallNaluProducesOnePacket) {
  camera_toolkit::RTPPackerParams params;
  params.maxPacketLength = 1400;
  camera_toolkit::RTPPacker packer(params);

  // 构造一个 10 字节负载的 IDR NAL（type=5）
  auto naluData = makeNalu(5, 10);
  camera_toolkit::Buffer buf(naluData.data(), static_cast<int>(naluData.size()));

  packer.put(buf);

  auto pkt1 = packer.get();
  ASSERT_TRUE(pkt1.has_value());

  // 单NALU包，marker bit 必须置1
  EXPECT_TRUE(getRTPMarker(*pkt1));
  // Payload type 应为 96（H.264）
  EXPECT_EQ(getRTPPayloadType(*pkt1), 96);

  // 不应有第二个包
  auto pkt2 = packer.get();
  EXPECT_FALSE(pkt2.has_value());
}

// ============================================================================
// 大 NAL 单元 FU-A 分片测试
// ============================================================================

TEST(RTPPackerTest, LargeNaluProducesMultipleFUAPackets) {
  camera_toolkit::RTPPackerParams params;
  params.maxPacketLength = 100;
  camera_toolkit::RTPPacker packer(params);

  // 构造一个 310 字节负载的 NAL（远大于 maxPacketLength=100）
  auto naluData = makeNalu(1, 310);
  camera_toolkit::Buffer buf(naluData.data(), static_cast<int>(naluData.size()));

  packer.put(buf);

  int packetCount = 0;
  bool lastMarkerSeen = false;
  camera_toolkit::Buffer lastPkt;
  uint16_t prevSeqNo = 0;
  bool firstPacket = true;

  while (auto pkt = packer.get()) {
    if (!firstPacket) {
      // 序列号应递增
      uint16_t seqNo = getRTPSeqNo(*pkt);
      EXPECT_EQ(static_cast<uint16_t>(prevSeqNo + 1), seqNo);
    }
    prevSeqNo = getRTPSeqNo(*pkt);
    firstPacket = false;
    lastMarkerSeen = getRTPMarker(*pkt);
    lastPkt = *pkt;
    packetCount++;
  }

  // 310 字节负载 + 1字节NAL头 = 311 字节，每片100字节 → ceil(311/100) = 4 包
  EXPECT_EQ(packetCount, 4);
  // 最后一个包的 marker bit 必须为 1
  EXPECT_TRUE(lastMarkerSeen);
}

// ============================================================================
// 序列号递增测试
// ============================================================================

TEST(RTPPackerTest, SequenceNumberIncrementsAcrossMultiplePuts) {
  camera_toolkit::RTPPackerParams params;
  params.maxPacketLength = 1400;
  camera_toolkit::RTPPacker packer(params);

  std::vector<uint16_t> seqNos;

  for (int i = 0; i < 3; ++i) {
    auto naluData = makeNalu(1, 10);
    camera_toolkit::Buffer buf(naluData.data(), static_cast<int>(naluData.size()));
    packer.put(buf);

    while (auto pkt = packer.get()) {
      seqNos.push_back(getRTPSeqNo(*pkt));
    }
  }

  ASSERT_EQ(seqNos.size(), 3u);
  // 序列号应严格递增
  for (size_t i = 1; i < seqNos.size(); ++i) {
    EXPECT_EQ(static_cast<uint16_t>(seqNos[i - 1] + 1), seqNos[i]);
  }
}

// ============================================================================
// 多 NAL 单元连续测试（同一 buffer 包含多个 NAL）
// ============================================================================

TEST(RTPPackerTest, MultipleNalusInOnePut) {
  camera_toolkit::RTPPackerParams params;
  params.maxPacketLength = 1400;
  camera_toolkit::RTPPacker packer(params);

  // 构造两个 NAL 单元拼接在一起（4字节起始码）
  auto nalu1 = makeNalu(7, 5);   // SPS type=7
  auto nalu2 = makeNalu(8, 3);   // PPS type=8

  std::vector<uint8_t> combined;
  combined.insert(combined.end(), nalu1.begin(), nalu1.end());
  combined.insert(combined.end(), nalu2.begin(), nalu2.end());

  camera_toolkit::Buffer buf(combined.data(), static_cast<int>(combined.size()));
  packer.put(buf);

  int count = 0;
  while (auto pkt = packer.get()) {
    count++;
  }

  // 两个小 NAL，应产生两个 RTP 包
  EXPECT_EQ(count, 2);
}

// ============================================================================
// 包大小合理性测试
// ============================================================================

TEST(RTPPackerTest, PacketSizeDoesNotExceedLimit) {
  camera_toolkit::RTPPackerParams params;
  params.maxPacketLength = 200;
  camera_toolkit::RTPPacker packer(params);

  // 500 字节负载
  auto naluData = makeNalu(1, 500);
  camera_toolkit::Buffer buf(naluData.data(), static_cast<int>(naluData.size()));
  packer.put(buf);

  while (auto pkt = packer.get()) {
    // 每个 RTP 包大小不应超过 maxPacketLength + RTP头(12) + FU头(2)
    EXPECT_LE(pkt->size, params.maxPacketLength + 14);
  }
}
