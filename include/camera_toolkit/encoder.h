/**
 * @file encoder.h
 * @brief H264编码器类定义
 *
 * 使用FFmpeg的libavcodec将YUV420帧编码为H264
 */
#pragma once

#include <memory>
#include <optional>

#include "common.h"

namespace camera_toolkit {

/**
 * @brief 编码配置参数结构体
 */
struct EncoderParams {
  int srcWidth = 640;            /**< 源图像宽度 */
  int srcHeight = 480;           /**< 源图像高度 */
  int encWidth = 640;            /**< 编码图像宽度 */
  int encHeight = 480;           /**< 编码图像高度 */
  int fps = 15;                  /**< 帧率 */
  int bitrate = 1000;            /**< 码率(kbps)，0表示不进行码率控制 */
  int gop = 12;                  /**< GOP大小 */
  bool chromaInterleave = false; /**< 色度是否交织 */
};

/**
 * @brief 编码后的帧结构体
 */
struct EncodedFrame {
  Buffer buffer;                        /**< 编码数据 */
  PictureType type = PictureType::None; /**< 帧类型 */

  /**
   * @brief 检查帧是否为空
   * @return 为空返回true
   */
  bool empty() const { return buffer.empty(); }
};

/**
 * @class Encoder
 * @brief H264编码器类
 *
 * 使用FFmpeg的libavcodec将YUV420帧编码为H264
 */
class Encoder : public NonCopyable {
 public:
  /**
   * @brief 构造函数
   * @param params 编码参数
   * @throws EncodeException 初始化失败时抛出
   */
  explicit Encoder(const EncoderParams& params);

  /**
   * @brief 析构函数
   */
  ~Encoder();

  /**
   * @brief 获取H264头信息(SPS, PPS)
   * @return 包含头信息的EncodedFrame，无更多头时返回nullopt
   *
   * @note 重复调用直到返回nullopt以获取所有头信息
   * @note 应在每次encode()调用前调用
   */
  std::optional<EncodedFrame> getHeaders();

  /**
   * @brief 编码一帧
   * @param input 包含YUV420数据的输入缓冲区
   * @return 包含编码数据的EncodedFrame
   * @throws EncodeException 发生错误时抛出
   */
  EncodedFrame encode(const Buffer& input);

  /**
   * @brief 设置量化参数
   * @param qp QP值(H264为0-51)
   * @return 成功返回true
   *
   * @note 仅在禁用码率控制(bitrate=0)时有效
   */
  bool setQP(int qp);

  /**
   * @brief 设置GOP大小
   * @param gop 新的GOP大小
   * @return 成功返回true
   */
  bool setGOP(int gop);

  /**
   * @brief 设置码率
   * @param bitrate 新码率(kbps, 0-32767)
   * @return 成功返回true
   */
  bool setBitrate(int bitrate);

  /**
   * @brief 设置帧率
   * @param fps 新帧率(必须>0)
   * @return 成功返回true
   */
  bool setFramerate(int fps);

  /**
   * @brief 强制下一帧为I帧
   *
   * @note 在getHeaders()和encode()之前调用以强制生成I帧
   */
  void forceIFrame();

  /**
   * @brief 获取编码参数
   * @return 编码参数引用
   */
  const EncoderParams& getParams() const;

 private:
  class Impl;                   /**< 前向声明实现类 */
  std::unique_ptr<Impl> pImpl_; /**< PIMPL指针 */
};

}  // namespace camera_toolkit
