/**
 * @file encoder.cpp
 * @brief H264编码器类实现
 */
#include "camera_toolkit/encoder.h"

#include <cassert>
#include <cstring>
#include <iostream>

#include "ffmpeg_common.h"

namespace camera_toolkit {

/**
 * @brief Encoder类的PIMPL实现
 */
class Encoder::Impl {
 public:
  /**
   * @brief 构造函数
   * @param params 编码参数
   * @throws EncodeException 初始化失败时抛出
   */
  explicit Impl(const EncoderParams& params) : params_(params) {
    // 查找编码器
    codec_ = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec_) {
      throw EncodeException("H264 codec not found");
    }

    // 分配编码上下文
    ctx_ = avcodec_alloc_context3(codec_);
    if (!ctx_) {
      throw EncodeException("Could not allocate video codec context");
    }

    // 配置编码器
    ctx_->bit_rate = params_.bitrate * 1000;  // 转换为bps
    ctx_->width = params_.srcWidth;
    ctx_->height = params_.srcHeight;
    ctx_->time_base = AVRational{1, params_.fps};
    ctx_->framerate = AVRational{params_.fps, 1};
    ctx_->gop_size = params_.gop;
    ctx_->max_b_frames = 1;
    ctx_->pix_fmt = AV_PIX_FMT_YUV420P;

    // 设置低延迟选项
    av_opt_set(ctx_->priv_data, "preset", "ultrafast", 0);
    av_opt_set(ctx_->priv_data, "tune", "zerolatency", 0);
    av_opt_set(ctx_->priv_data, "x264opts", "no-mbtree:sliced-threads:sync-lookahead=0", 0);

    // 打开编码器
    if (avcodec_open2(ctx_, codec_, nullptr) < 0) {
      avcodec_free_context(&ctx_);
      throw EncodeException("Could not open codec");
    }

    // 分配帧
    frame_ = av_frame_alloc();
    if (!frame_) {
      avcodec_free_context(&ctx_);
      throw EncodeException("Could not allocate video frame");
    }

    frame_->format = ctx_->pix_fmt;
    frame_->width = ctx_->width;
    frame_->height = ctx_->height;

    inBufferSize_ = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, params_.srcWidth, params_.srcHeight, 1);
    inBuffer_ = static_cast<uint8_t*>(av_malloc(inBufferSize_));
    if (!inBuffer_) {
      av_frame_free(&frame_);
      avcodec_free_context(&ctx_);
      throw EncodeException("Could not allocate input buffer");
    }

    av_image_fill_arrays(frame_->data, frame_->linesize, inBuffer_, AV_PIX_FMT_YUV420P, params_.srcWidth,
                         params_.srcHeight, 1);

    // 初始化数据包
    packet_ = av_packet_alloc();
    if (!packet_) {
      av_free(inBuffer_);
      av_frame_free(&frame_);
      avcodec_free_context(&ctx_);
      throw EncodeException("Could not allocate packet");
    }

    std::cout << "+++ Encoder opened" << std::endl;
  }

  /**
   * @brief 析构函数
   */
  ~Impl() {
    if (packet_) av_packet_free(&packet_);
    if (inBuffer_) av_free(inBuffer_);
    if (frame_) av_frame_free(&frame_);
    if (ctx_) avcodec_free_context(&ctx_);

    std::cout << "+++ Encoder closed" << std::endl;
  }

  /**
   * @brief 获取H264头信息(SPS, PPS)
   * @return 包含头信息的EncodedFrame，无更多头时返回nullopt
   *
   * @note FFmpeg的x264编码器通常将SPS/PPS嵌入到比特流中
   * @note 此函数保留用于API兼容性
   */
  std::optional<EncodedFrame> getHeaders() { return std::nullopt; }

  /**
   * @brief 编码一帧
   * @param input 包含YUV420数据的输入缓冲区
   * @return 包含编码数据的EncodedFrame
   * @throws EncodeException 编码失败时抛出
   */
  EncodedFrame encode(const Buffer& input) {
    assert(input.size == inBufferSize_);

    // 复制输入到帧缓冲区
    std::memcpy(inBuffer_, input.data, input.size);
    frame_->pts = frameCounter_++;

    // 发送帧到编码器
    int ret = avcodec_send_frame(ctx_, frame_);

    // 重置关键帧标志
    frame_->pict_type = AV_PICTURE_TYPE_NONE;
    frame_->key_frame = 0;

    if (ret < 0) {
      throw EncodeException("Error sending frame for encoding");
    }

    // 接收编码后的数据包
    ret = avcodec_receive_packet(ctx_, packet_);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      std::cerr << "!!! Encoded frame delayed" << std::endl;
      return EncodedFrame{};
    } else if (ret < 0) {
      throw EncodeException("Error during encoding");
    }

    EncodedFrame result;
    result.buffer = Buffer(packet_->data, packet_->size);

    // 确定帧类型
    if (packet_->flags & AV_PKT_FLAG_KEY) {
      result.type = PictureType::I;
    } else {
      // 通过检查DTS与PTS判断B帧
      if (packet_->dts != packet_->pts && packet_->dts < packet_->pts) {
        result.type = PictureType::B;
      } else {
        result.type = PictureType::P;
      }
    }

    return result;
  }

  /**
   * @brief 设置量化参数
   * @param qp QP值(H264为0-51)
   * @return 成功返回true
   *
   * @note 此函数未实现
   */
  bool setQP(int /*qp*/) {
    std::cerr << "*** setQP: This function is not implemented" << std::endl;
    return false;
  }

  /**
   * @brief 设置GOP大小
   * @param gop 新的GOP大小
   * @return 成功返回true
   */
  bool setGOP(int gop) {
    ctx_->gop_size = gop;
    return true;
  }

  /**
   * @brief 设置码率
   * @param bitrate 新码率(kbps)
   * @return 成功返回true
   */
  bool setBitrate(int bitrate) {
    ctx_->bit_rate = bitrate * 1000;
    return true;
  }

  /**
   * @brief 设置帧率
   * @param fps 新帧率
   * @return 成功返回true
   */
  bool setFramerate(int fps) {
    ctx_->time_base = AVRational{1, fps};
    ctx_->framerate = AVRational{fps, 1};
    return true;
  }

  /**
   * @brief 强制下一帧为I帧
   */
  void forceIFrame() {
    frame_->pict_type = AV_PICTURE_TYPE_I;
    frame_->key_frame = 1;
  }

  /**
   * @brief 获取编码参数
   * @return 编码参数引用
   */
  const EncoderParams& getParams() const { return params_; }

 private:
  EncoderParams params_;           /**< 编码参数 */
  const AVCodec* codec_ = nullptr; /**< 编解码器 */
  AVCodecContext* ctx_ = nullptr;  /**< 编码上下文 */
  AVFrame* frame_ = nullptr;       /**< 帧 */
  AVPacket* packet_ = nullptr;     /**< 数据包 */
  uint8_t* inBuffer_ = nullptr;    /**< 输入缓冲区 */
  int inBufferSize_ = 0;           /**< 输入缓冲区大小 */
  unsigned long frameCounter_ = 0; /**< 帧计数器 */
};

// ============================================================================
// 公共接口实现
// ============================================================================

Encoder::Encoder(const EncoderParams& params) : pImpl_(std::make_unique<Impl>(params)) {}

Encoder::~Encoder() = default;

std::optional<EncodedFrame> Encoder::getHeaders() { return pImpl_->getHeaders(); }

EncodedFrame Encoder::encode(const Buffer& input) { return pImpl_->encode(input); }

bool Encoder::setQP(int qp) { return pImpl_->setQP(qp); }

bool Encoder::setGOP(int gop) { return pImpl_->setGOP(gop); }

bool Encoder::setBitrate(int bitrate) { return pImpl_->setBitrate(bitrate); }

bool Encoder::setFramerate(int fps) { return pImpl_->setFramerate(fps); }

void Encoder::forceIFrame() { pImpl_->forceIFrame(); }

const EncoderParams& Encoder::getParams() const { return pImpl_->getParams(); }

}  // namespace camera_toolkit
