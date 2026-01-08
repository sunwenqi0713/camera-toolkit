/**
 * @file timestamp.h
 * @brief 时间戳绘制类定义
 *
 * 在视频帧上绘制时间戳和自定义文本
 */
#pragma once

#include <cstdint>
#include <memory>

#include "common.h"

namespace camera_toolkit {

/**
 * @brief 时间戳绘制配置参数结构体
 */
struct TimestampParams {
  int startX = 10;      /**< 距左边缘的距离(像素) */
  int startY = 10;      /**< 距上边缘的距离(像素) */
  int videoWidth = 640; /**< 视频帧宽度 */
  int factor = 0;       /**< 文字大小(0=小, 1=大) */
};

/**
 * @class Timestamp
 * @brief 时间戳绘制类
 *
 * 在视频帧上绘制时间戳
 */
class Timestamp : public NonCopyable {
 public:
  /**
   * @brief 构造函数
   * @param params 时间戳参数
   */
  explicit Timestamp(const TimestampParams& params);

  /**
   * @brief 析构函数
   */
  ~Timestamp();

  /**
   * @brief 在图像上绘制时间戳
   * @param image 图像数据指针(YUV的Y平面)
   *
   * @note 时间戳格式: "YYYY-MM-DD HH:MM:SS (TZ)"
   */
  void draw(uint8_t* image);

  /**
   * @brief 在图像上绘制自定义文字
   * @param image 图像数据指针
   * @param text 要绘制的文字
   */
  void drawText(uint8_t* image, const char* text);

  /**
   * @brief 获取时间戳参数
   * @return 时间戳参数引用
   */
  const TimestampParams& getParams() const;

 private:
  class Impl;                   /**< 前向声明实现类 */
  std::unique_ptr<Impl> pImpl_; /**< PIMPL指针 */
};

}  // namespace camera_toolkit
