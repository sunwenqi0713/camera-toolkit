/**
 * @file convert.h
 * @brief 图像格式转换类定义
 *
 * 使用FFmpeg的swscale进行不同像素格式和分辨率之间的转换
 */
#pragma once

#include <memory>

#include "common.h"

namespace camera_toolkit {

/**
 * @brief 转换配置参数结构体
 */
struct ConvertParams {
  int inWidth = 640;                                /**< 输入图像宽度 */
  int inHeight = 480;                               /**< 输入图像高度 */
  PixelFormat inPixelFormat = PixelFormat::YUYV;    /**< 输入像素格式 */
  int outWidth = 640;                               /**< 输出图像宽度 */
  int outHeight = 480;                              /**< 输出图像高度 */
  PixelFormat outPixelFormat = PixelFormat::YUV420; /**< 输出像素格式 */
};

/**
 * @class Convert
 * @brief 图像格式转换类
 *
 * 使用FFmpeg的swscale进行不同像素格式和分辨率之间的转换
 */
class Convert : public NonCopyable {
 public:
  /**
   * @brief 构造函数
   * @param params 转换参数
   * @throws ConvertException 初始化失败时抛出
   */
  explicit Convert(const ConvertParams& params);

  /**
   * @brief 析构函数
   */
  ~Convert();

  /**
   * @brief 转换图像
   * @param input 输入缓冲区
   * @return 包含转换后图像的Buffer
   * @throws ConvertException 发生错误时抛出
   */
  Buffer convert(const Buffer& input);

  /**
   * @brief 获取转换参数
   * @return 转换参数引用
   */
  const ConvertParams& getParams() const;

  /**
   * @brief 获取输出缓冲区大小
   * @return 输出缓冲区大小(字节)
   */
  int getOutputSize() const;

 private:
  class Impl;                   /**< 前向声明实现类 */
  std::unique_ptr<Impl> pImpl_; /**< PIMPL指针 */
};

}  // namespace camera_toolkit
