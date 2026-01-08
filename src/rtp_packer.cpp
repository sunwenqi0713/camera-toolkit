/**
 * @file rtp_packer.cpp
 * @brief RTP打包器类实现
 */
#include "camera_toolkit/rtp_packer.h"

#include <arpa/inet.h>
#include <sys/time.h>

#include <cstring>
#include <iostream>
#include <vector>

namespace camera_toolkit {

namespace {

constexpr int H264_PAYLOAD_TYPE = 96;      /**< H264负载类型 */
constexpr int MAX_OUTBUF_SIZE = 10 * 1024; /**< 最大输出缓冲区大小(10KB) */

/**
 * @brief RTP头结构体 (小端字节序位域)
 */
#pragma pack(push, 1)
struct RTPHeader {
  // 字节0
  uint8_t csrcLen : 4;   /**< CSRC计数 */
  uint8_t extension : 1; /**< 扩展位 */
  uint8_t padding : 1;   /**< 填充位 */
  uint8_t version : 2;   /**< 版本(2) */
  // 字节1
  uint8_t payload : 7; /**< 负载类型 */
  uint8_t marker : 1;  /**< 标记位 */
  // 字节2-3
  uint16_t seqNo; /**< 序列号 */
  // 字节4-7
  uint32_t timestamp; /**< 时间戳 */
  // 字节8-11
  uint32_t ssrc; /**< 同步源标识符 */
};

/**
 * @brief NAL单元头结构体
 */
struct NALUHeader {
  uint8_t type : 5; /**< NAL单元类型 */
  uint8_t nri : 2;  /**< nal_ref_idc */
  uint8_t f : 1;    /**< forbidden_zero_bit */
};

/**
 * @brief FU指示符结构体
 */
struct FUIndicator {
  uint8_t type : 5; /**< 类型(28=FU-A) */
  uint8_t nri : 2;  /**< nal_ref_idc */
  uint8_t f : 1;    /**< forbidden_zero_bit */
};

/**
 * @brief FU头结构体
 */
struct FUHeader {
  uint8_t type : 5; /**< NAL单元类型 */
  uint8_t r : 1;    /**< 保留位 */
  uint8_t e : 1;    /**< 结束位 */
  uint8_t s : 1;    /**< 起始位 */
};
#pragma pack(pop)

/**
 * @brief NAL单元结构体
 */
struct NALU {
  int startCodeLen = 0; /**< 起始码长度 */
  char* data = nullptr; /**< 数据指针 */
  int len = 0;          /**< 数据长度 */
  int forbiddenBit = 0; /**< 禁止位 */
  int nalRefIdc = 0;    /**< nal_ref_idc */
  int nalUnitType = 0;  /**< NAL单元类型 */
};

/**
 * @brief 获取当前毫秒时间
 * @return 当前毫秒时间戳
 */
uint32_t getCurrentMillisec() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return static_cast<uint32_t>(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

/**
 * @brief 检查是否为4字节起始码
 * @param buf 缓冲区指针
 * @return 是4字节起始码返回true
 */
bool isStartCode4(const char* buf) { return buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1; }

/**
 * @brief 检查是否为3字节起始码
 * @param buf 缓冲区指针
 * @return 是3字节起始码返回true
 */
bool isStartCode3(const char* buf) { return buf[0] == 0 && buf[1] == 0 && buf[2] == 1; }

}  // anonymous namespace

/**
 * @brief RTPPacker类的PIMPL实现
 */
class RTPPacker::Impl {
 public:
  /**
   * @brief 构造函数
   * @param params 打包器参数
   */
  explicit Impl(const RTPPackerParams& params) : params_(params) {
    outBuffer_.resize(MAX_OUTBUF_SIZE);
    tsStartMillisec_ = getCurrentMillisec();

    std::cout << "+++ RTPPacker opened" << std::endl;
  }

  /**
   * @brief 析构函数
   */
  ~Impl() { std::cout << "+++ RTPPacker closed" << std::endl; }

  /**
   * @brief 放入待打包的NAL单元
   * @param input 包含一个或多个NAL单元的缓冲区
   */
  void put(const Buffer& input) {
    inBuffer_ = static_cast<char*>(input.data);
    nextNaluPtr_ = inBuffer_;
    inBufferSize_ = input.size;
    fuCounter_ = 0;
    lastFuSize_ = 0;
    fuIndex_ = 0;
    inBufferComplete_ = false;
    naluComplete_ = true;  // 开始新的NAL单元
  }

  /**
   * @brief 获取下一个RTP包
   * @return 包含RTP包的Buffer，无更多包时返回nullopt
   * @throws PackException 缓冲区溢出时抛出
   */
  std::optional<Buffer> get() {
    if (inBufferComplete_) {
      return std::nullopt;
    }

    // 清空输出缓冲区
    std::memset(outBuffer_.data(), 0, MAX_OUTBUF_SIZE);
    char* tmpOutbuf = outBuffer_.data();

    // 设置通用RTP头
    auto* rtpHdr = reinterpret_cast<RTPHeader*>(tmpOutbuf);
    rtpHdr->payload = H264_PAYLOAD_TYPE;
    rtpHdr->version = 2;
    rtpHdr->marker = 0;
    rtpHdr->ssrc = htonl(params_.ssrc);

    if (naluComplete_) {
      // 当前NAL单元完成，查找下一个NAL单元
      int ret = getNextNalu();
      if (ret <= 0) {
        inBufferComplete_ = true;
        return std::nullopt;
      }

      rtpHdr->seqNo = htons(seqNum_++);
      tsCurrentSample_ = static_cast<uint32_t>((getCurrentMillisec() - tsStartMillisec_) * 90.0);
      rtpHdr->timestamp = htonl(tsCurrentSample_);

      if (nalu_.len <= params_.maxPacketLength) {
        // 无需分片
        rtpHdr->marker = 1;

        auto* naluHdr = reinterpret_cast<NALUHeader*>(tmpOutbuf + 12);
        naluHdr->f = nalu_.forbiddenBit;
        naluHdr->nri = nalu_.nalRefIdc;
        naluHdr->type = nalu_.nalUnitType;

        char* naluPayload = tmpOutbuf + 13;  // 12字节RTP + 1字节NALU头
        int outSize = nalu_.len + 12;

        if (MAX_OUTBUF_SIZE < outSize) {
          throw PackException("RTP output buffer overflow");
        }

        std::memcpy(naluPayload, nalu_.data + 1, nalu_.len - 1);  // 排除NALU头
        naluComplete_ = true;

        return Buffer(outBuffer_.data(), outSize);
      } else {
        // 需要分片
        if (nalu_.len % params_.maxPacketLength == 0) {
          fuCounter_ = nalu_.len / params_.maxPacketLength - 1;
          lastFuSize_ = params_.maxPacketLength;
        } else {
          fuCounter_ = nalu_.len / params_.maxPacketLength;
          lastFuSize_ = nalu_.len % params_.maxPacketLength;
        }
        fuIndex_ = 0;

        // 第一个FU
        rtpHdr->marker = 0;

        auto* fuInd = reinterpret_cast<FUIndicator*>(tmpOutbuf + 12);
        fuInd->f = nalu_.forbiddenBit;
        fuInd->nri = nalu_.nalRefIdc;
        fuInd->type = 28;  // FU-A

        auto* fuHdr = reinterpret_cast<FUHeader*>(tmpOutbuf + 13);
        fuHdr->e = 0;
        fuHdr->r = 0;
        fuHdr->s = 1;  // 起始位
        fuHdr->type = nalu_.nalUnitType;

        char* naluPayload = tmpOutbuf + 14;
        int outSize = params_.maxPacketLength + 14;

        if (MAX_OUTBUF_SIZE < outSize) {
          throw PackException("RTP output buffer overflow");
        }

        std::memcpy(naluPayload, nalu_.data + 1, params_.maxPacketLength);
        naluComplete_ = false;
        fuIndex_++;

        return Buffer(outBuffer_.data(), outSize);
      }
    } else {
      // 发送剩余的FU
      rtpHdr->seqNo = htons(seqNum_++);
      rtpHdr->timestamp = htonl(tsCurrentSample_);

      if (fuIndex_ == fuCounter_) {
        // 最后一个FU
        rtpHdr->marker = 1;

        auto* fuInd = reinterpret_cast<FUIndicator*>(tmpOutbuf + 12);
        fuInd->f = nalu_.forbiddenBit;
        fuInd->nri = nalu_.nalRefIdc;
        fuInd->type = 28;

        auto* fuHdr = reinterpret_cast<FUHeader*>(tmpOutbuf + 13);
        fuHdr->r = 0;
        fuHdr->s = 0;
        fuHdr->type = nalu_.nalUnitType;
        fuHdr->e = 1;  // 结束位

        char* naluPayload = tmpOutbuf + 14;
        int outSize = lastFuSize_ - 1 + 14;

        if (MAX_OUTBUF_SIZE < outSize) {
          throw PackException("RTP output buffer overflow");
        }

        std::memcpy(naluPayload, nalu_.data + 1 + fuIndex_ * params_.maxPacketLength, lastFuSize_ - 1);

        naluComplete_ = true;
        fuIndex_ = 0;

        return Buffer(outBuffer_.data(), outSize);
      } else {
        // 中间FU
        rtpHdr->marker = 0;

        auto* fuInd = reinterpret_cast<FUIndicator*>(tmpOutbuf + 12);
        fuInd->f = nalu_.forbiddenBit;
        fuInd->nri = nalu_.nalRefIdc;
        fuInd->type = 28;

        auto* fuHdr = reinterpret_cast<FUHeader*>(tmpOutbuf + 13);
        fuHdr->r = 0;
        fuHdr->s = 0;
        fuHdr->type = nalu_.nalUnitType;
        fuHdr->e = 0;

        char* naluPayload = tmpOutbuf + 14;
        int outSize = params_.maxPacketLength + 14;

        if (MAX_OUTBUF_SIZE < outSize) {
          throw PackException("RTP output buffer overflow");
        }

        std::memcpy(naluPayload, nalu_.data + 1 + fuIndex_ * params_.maxPacketLength, params_.maxPacketLength);

        fuIndex_++;

        return Buffer(outBuffer_.data(), outSize);
      }
    }
  }

  /**
   * @brief 获取打包器参数
   * @return 打包器参数引用
   */
  const RTPPackerParams& getParams() const { return params_; }

 private:
  /**
   * @brief 获取下一个NAL单元
   * @return 成功返回1，失败返回0或负值
   */
  int getNextNalu() {
    if (!nextNaluPtr_) {
      return 0;
    }

    // 检查起始码
    if (isStartCode3(nextNaluPtr_)) {
      nalu_.startCodeLen = 3;
    } else if (isStartCode4(nextNaluPtr_)) {
      nalu_.startCodeLen = 4;
    } else {
      std::cerr << "!!! No start code found" << std::endl;
      return -1;
    }

    // 查找下一个起始码
    char* curNaluPtr = nextNaluPtr_;
    char* nextPtr = curNaluPtr + nalu_.startCodeLen;

    while (true) {
      nextPtr++;

      if (nextPtr - inBuffer_ >= inBufferSize_) {
        nextNaluPtr_ = nullptr;
        break;
      }

      if (isStartCode3(nextPtr) || isStartCode4(nextPtr)) {
        nextNaluPtr_ = nextPtr;
        break;
      }
    }

    nalu_.data = curNaluPtr + nalu_.startCodeLen;
    nalu_.len = nextPtr - curNaluPtr - nalu_.startCodeLen;
    nalu_.forbiddenBit = (nalu_.data[0] & 0x80) >> 7;
    nalu_.nalRefIdc = (nalu_.data[0] & 0x60) >> 5;
    nalu_.nalUnitType = nalu_.data[0] & 0x1f;

    return 1;
  }

  RTPPackerParams params_;        /**< 打包器参数 */
  std::vector<char> outBuffer_;   /**< 输出缓冲区 */
  char* inBuffer_ = nullptr;      /**< 输入缓冲区 */
  char* nextNaluPtr_ = nullptr;   /**< 下一个NAL单元指针 */
  int inBufferSize_ = 0;          /**< 输入缓冲区大小 */
  int fuCounter_ = 0;             /**< FU计数器 */
  int lastFuSize_ = 0;            /**< 最后一个FU大小 */
  int fuIndex_ = 0;               /**< 当前FU索引 */
  bool inBufferComplete_ = false; /**< 输入缓冲区是否处理完毕 */
  bool naluComplete_ = true;      /**< 当前NAL单元是否完成 */
  NALU nalu_;                     /**< 当前NAL单元 */
  uint16_t seqNum_ = 0;           /**< 序列号 */
  uint32_t tsStartMillisec_ = 0;  /**< 起始毫秒时间戳 */
  uint32_t tsCurrentSample_ = 0;  /**< 当前采样时间戳 */
};

// ============================================================================
// 公共接口实现
// ============================================================================

RTPPacker::RTPPacker(const RTPPackerParams& params) : pImpl_(std::make_unique<Impl>(params)) {}

RTPPacker::~RTPPacker() = default;

void RTPPacker::put(const Buffer& input) { pImpl_->put(input); }

std::optional<Buffer> RTPPacker::get() { return pImpl_->get(); }

const RTPPackerParams& RTPPacker::getParams() const { return pImpl_->getParams(); }

}  // namespace camera_toolkit
