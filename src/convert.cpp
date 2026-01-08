/**
 * @file convert.cpp
 * @brief 图像格式转换类实现
 */
#include "camera_toolkit/convert.h"

#include <cassert>
#include <cstring>
#include <iostream>

#include "ffmpeg_common.h"

namespace camera_toolkit {

/**
 * @brief Convert类的PIMPL实现
 */
class Convert::Impl {
 public:
  /**
   * @brief 构造函数
   * @param params 转换参数
   * @throws ConvertException 初始化失败时抛出
   */
  explicit Impl(const ConvertParams& params) : params_(params) {
    inAVFormat_ = toAVPixelFormat(params_.inPixelFormat);
    outAVFormat_ = toAVPixelFormat(params_.outPixelFormat);

    if (inAVFormat_ == AV_PIX_FMT_NONE || outAVFormat_ == AV_PIX_FMT_NONE) {
      throw ConvertException("Unsupported pixel format");
    }

    // 创建缩放上下文
    swsCtx_ = sws_getContext(params_.inWidth, params_.inHeight, inAVFormat_, params_.outWidth, params_.outHeight,
                             outAVFormat_, SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!swsCtx_) {
      throw ConvertException("Failed to create scale context");
    }

    // 分配源帧
    srcFrame_ = av_frame_alloc();
    if (!srcFrame_) {
      sws_freeContext(swsCtx_);
      throw ConvertException("Failed to allocate source frame");
    }

    srcBufferSize_ = av_image_get_buffer_size(inAVFormat_, params_.inWidth, params_.inHeight, 1);
    srcBuffer_ = static_cast<uint8_t*>(av_malloc(srcBufferSize_));
    if (!srcBuffer_) {
      av_frame_free(&srcFrame_);
      sws_freeContext(swsCtx_);
      throw ConvertException("Failed to allocate source buffer");
    }

    av_image_fill_arrays(srcFrame_->data, srcFrame_->linesize, srcBuffer_, inAVFormat_, params_.inWidth,
                         params_.inHeight, 1);

    // 分配目标帧
    dstFrame_ = av_frame_alloc();
    if (!dstFrame_) {
      av_free(srcBuffer_);
      av_frame_free(&srcFrame_);
      sws_freeContext(swsCtx_);
      throw ConvertException("Failed to allocate destination frame");
    }

    dstBufferSize_ = av_image_get_buffer_size(outAVFormat_, params_.outWidth, params_.outHeight, 1);
    dstBuffer_ = static_cast<uint8_t*>(av_malloc(dstBufferSize_));
    if (!dstBuffer_) {
      av_frame_free(&dstFrame_);
      av_free(srcBuffer_);
      av_frame_free(&srcFrame_);
      sws_freeContext(swsCtx_);
      throw ConvertException("Failed to allocate destination buffer");
    }

    av_image_fill_arrays(dstFrame_->data, dstFrame_->linesize, dstBuffer_, outAVFormat_, params_.outWidth,
                         params_.outHeight, 1);

    std::cout << "+++ Convert opened" << std::endl;
  }

  /**
   * @brief 析构函数
   */
  ~Impl() {
    if (dstBuffer_) av_free(dstBuffer_);
    if (dstFrame_) av_frame_free(&dstFrame_);
    if (srcBuffer_) av_free(srcBuffer_);
    if (srcFrame_) av_frame_free(&srcFrame_);
    if (swsCtx_) sws_freeContext(swsCtx_);

    std::cout << "+++ Convert closed" << std::endl;
  }

  /**
   * @brief 转换图像
   * @param input 输入缓冲区
   * @return 包含转换后图像的Buffer
   */
  Buffer convert(const Buffer& input) {
    assert(input.size == srcBufferSize_);

    std::memcpy(srcBuffer_, input.data, input.size);

    sws_scale(swsCtx_, srcFrame_->data, srcFrame_->linesize, 0, params_.inHeight, dstFrame_->data, dstFrame_->linesize);

    return Buffer(dstBuffer_, dstBufferSize_);
  }

  /**
   * @brief 获取转换参数
   * @return 转换参数引用
   */
  const ConvertParams& getParams() const { return params_; }

  /**
   * @brief 获取输出缓冲区大小
   * @return 输出缓冲区大小(字节)
   */
  int getOutputSize() const { return dstBufferSize_; }

 private:
  ConvertParams params_;                        /**< 转换参数 */
  SwsContext* swsCtx_ = nullptr;                /**< swscale上下文 */
  AVFrame* srcFrame_ = nullptr;                 /**< 源帧 */
  AVFrame* dstFrame_ = nullptr;                 /**< 目标帧 */
  uint8_t* srcBuffer_ = nullptr;                /**< 源缓冲区 */
  uint8_t* dstBuffer_ = nullptr;                /**< 目标缓冲区 */
  int srcBufferSize_ = 0;                       /**< 源缓冲区大小 */
  int dstBufferSize_ = 0;                       /**< 目标缓冲区大小 */
  AVPixelFormat inAVFormat_ = AV_PIX_FMT_NONE;  /**< 输入FFmpeg格式 */
  AVPixelFormat outAVFormat_ = AV_PIX_FMT_NONE; /**< 输出FFmpeg格式 */
};

// ============================================================================
// 公共接口实现
// ============================================================================

Convert::Convert(const ConvertParams& params) : pImpl_(std::make_unique<Impl>(params)) {}

Convert::~Convert() = default;

Buffer Convert::convert(const Buffer& input) { return pImpl_->convert(input); }

const ConvertParams& Convert::getParams() const { return pImpl_->getParams(); }

int Convert::getOutputSize() const { return pImpl_->getOutputSize(); }

}  // namespace camera_toolkit
