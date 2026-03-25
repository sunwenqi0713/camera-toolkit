/**
 * @file rtp_packer.cpp
 * @brief RTP打包器类实现
 */
#include "camera_toolkit/rtp_packer.h"

#include <arpa/inet.h>
#include <sys/time.h>

#include <cstring>
#include <vector>

#include "log.h"

namespace camera_toolkit {

namespace {

constexpr int H264_PAYLOAD_TYPE = 96;           /**< H264负载类型 */
constexpr int MAX_OUTBUF_SIZE = 10 * 1024;      /**< 最大输出缓冲区大小(10KB) */
constexpr int RTP_HEADER_SIZE = 12;             /**< RTP固定头大小(字节) */
constexpr int SINGLE_NALU_PAYLOAD_OFFSET = 13;  /**< 单NALU负载偏移: RTP(12)+NALU头(1) */
constexpr int FU_HEADERS_SIZE = 14;             /**< FU-A总头大小: RTP(12)+FU指示符(1)+FU头(1) */
constexpr uint8_t FU_A_TYPE = 28;               /**< FU-A类型值 */

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

    log::info("RTPPacker opened");
  }

  /**
   * @brief 析构函数
   */
  ~Impl() { log::info("RTPPacker closed"); }

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
   * @throws PackException 缓冲区溢出或越界时抛出
   */
  std::optional<Buffer> get() {
    if (inBufferComplete_) {
      return std::nullopt;
    }

    std::memset(outBuffer_.data(), 0, MAX_OUTBUF_SIZE);
    char* outBuf = outBuffer_.data();

    auto* rtpHdr = reinterpret_cast<RTPHeader*>(outBuf);
    rtpHdr->payload = H264_PAYLOAD_TYPE;
    rtpHdr->version = 2;
    rtpHdr->marker = 0;
    rtpHdr->ssrc = htonl(params_.ssrc);

    if (naluComplete_) {
      if (getNextNalu() <= 0) {
        inBufferComplete_ = true;
        return std::nullopt;
      }

      rtpHdr->seqNo = htons(seqNum_++);
      tsCurrentSample_ = static_cast<uint32_t>((getCurrentMillisec() - tsStartMillisec_) * 90.0);
      rtpHdr->timestamp = htonl(tsCurrentSample_);

      if (nalu_.len <= params_.maxPacketLength) {
        return packSingleNAL(outBuf, rtpHdr);
      } else {
        return packFirstFU(outBuf, rtpHdr);
      }
    } else {
      rtpHdr->seqNo = htons(seqNum_++);
      rtpHdr->timestamp = htonl(tsCurrentSample_);

      if (fuIndex_ == fuCounter_) {
        return packLastFU(outBuf, rtpHdr);
      } else {
        return packMiddleFU(outBuf, rtpHdr);
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
   * @brief 填充FU-A指示符和头部
   * @param buf 写入位置指针（指向RTP头之后）
   * @param isStart 是否为起始分片
   * @param isEnd 是否为结束分片
   */
  void fillFUHeaders(char* buf, bool isStart, bool isEnd) {
    auto* fuInd = reinterpret_cast<FUIndicator*>(buf);
    fuInd->f = nalu_.forbiddenBit;
    fuInd->nri = nalu_.nalRefIdc;
    fuInd->type = FU_A_TYPE;

    auto* fuHdr = reinterpret_cast<FUHeader*>(buf + 1);
    fuHdr->r = 0;
    fuHdr->s = isStart ? 1 : 0;
    fuHdr->e = isEnd ? 1 : 0;
    fuHdr->type = nalu_.nalUnitType;
  }

  /**
   * @brief 打包单个完整NALU（无需分片）
   */
  Buffer packSingleNAL(char* outBuf, RTPHeader* rtpHdr) {
    rtpHdr->marker = 1;

    auto* naluHdr = reinterpret_cast<NALUHeader*>(outBuf + RTP_HEADER_SIZE);
    naluHdr->f = nalu_.forbiddenBit;
    naluHdr->nri = nalu_.nalRefIdc;
    naluHdr->type = nalu_.nalUnitType;

    int outSize = nalu_.len + RTP_HEADER_SIZE;
    if (outSize > MAX_OUTBUF_SIZE) {
      throw PackException("RTP output buffer overflow (single NALU, size=" + std::to_string(outSize) + ")");
    }

    // nalu_.data[0] 是NALU头字节，已写入naluHdr，跳过
    std::memcpy(outBuf + SINGLE_NALU_PAYLOAD_OFFSET, nalu_.data + 1, nalu_.len - 1);
    naluComplete_ = true;
    return Buffer(outBuf, outSize);
  }

  /**
   * @brief 打包FU-A第一个分片
   */
  Buffer packFirstFU(char* outBuf, RTPHeader* rtpHdr) {
    // 计算分片数量
    if (nalu_.len % params_.maxPacketLength == 0) {
      fuCounter_ = nalu_.len / params_.maxPacketLength - 1;
      lastFuSize_ = params_.maxPacketLength;
    } else {
      fuCounter_ = nalu_.len / params_.maxPacketLength;
      lastFuSize_ = nalu_.len % params_.maxPacketLength;
    }
    fuIndex_ = 0;

    rtpHdr->marker = 0;
    fillFUHeaders(outBuf + RTP_HEADER_SIZE, /*isStart=*/true, /*isEnd=*/false);

    int outSize = params_.maxPacketLength + FU_HEADERS_SIZE;
    if (outSize > MAX_OUTBUF_SIZE) {
      throw PackException("RTP output buffer overflow (first FU-A, size=" + std::to_string(outSize) + ")");
    }

    // 第一个分片跳过NALU头字节，从data[1]开始
    std::memcpy(outBuf + FU_HEADERS_SIZE, nalu_.data + 1, params_.maxPacketLength);
    naluComplete_ = false;
    fuIndex_++;
    return Buffer(outBuf, outSize);
  }

  /**
   * @brief 打包FU-A最后一个分片
   */
  Buffer packLastFU(char* outBuf, RTPHeader* rtpHdr) {
    if (lastFuSize_ < 1) {
      throw PackException("Invalid last FU-A size: " + std::to_string(lastFuSize_));
    }

    // 校验偏移不越界
    int payloadOffset = fuIndex_ * params_.maxPacketLength;
    if (payloadOffset + lastFuSize_ - 1 > nalu_.len - 1) {
      throw PackException("FU-A last fragment out of bounds (offset=" + std::to_string(payloadOffset) + ")");
    }

    rtpHdr->marker = 1;
    fillFUHeaders(outBuf + RTP_HEADER_SIZE, /*isStart=*/false, /*isEnd=*/true);

    int outSize = lastFuSize_ - 1 + FU_HEADERS_SIZE;
    if (outSize > MAX_OUTBUF_SIZE) {
      throw PackException("RTP output buffer overflow (last FU-A, size=" + std::to_string(outSize) + ")");
    }

    std::memcpy(outBuf + FU_HEADERS_SIZE, nalu_.data + 1 + payloadOffset, lastFuSize_ - 1);
    naluComplete_ = true;
    fuIndex_ = 0;
    return Buffer(outBuf, outSize);
  }

  /**
   * @brief 打包FU-A中间分片
   */
  Buffer packMiddleFU(char* outBuf, RTPHeader* rtpHdr) {
    // 校验偏移不越界
    int payloadOffset = fuIndex_ * params_.maxPacketLength;
    if (payloadOffset + params_.maxPacketLength > nalu_.len - 1) {
      throw PackException("FU-A middle fragment out of bounds (offset=" + std::to_string(payloadOffset) + ")");
    }

    rtpHdr->marker = 0;
    fillFUHeaders(outBuf + RTP_HEADER_SIZE, /*isStart=*/false, /*isEnd=*/false);

    int outSize = params_.maxPacketLength + FU_HEADERS_SIZE;
    if (outSize > MAX_OUTBUF_SIZE) {
      throw PackException("RTP output buffer overflow (middle FU-A, size=" + std::to_string(outSize) + ")");
    }

    std::memcpy(outBuf + FU_HEADERS_SIZE, nalu_.data + 1 + payloadOffset, params_.maxPacketLength);
    fuIndex_++;
    return Buffer(outBuf, outSize);
  }

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
      log::warn("No start code found in NAL unit data");
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
