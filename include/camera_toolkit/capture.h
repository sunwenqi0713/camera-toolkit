/**
 * @file capture.h
 * @brief V4L2视频采集类定义
 *
 * 提供从V4L2兼容设备(如USB摄像头)采集视频帧的功能
 */
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "common.h"

namespace camera_toolkit {

/**
 * @brief 采集配置参数结构体
 */
struct CaptureParams {
  std::string deviceName = "/dev/video0";      /**< 视频设备路径 */
  int width = 640;                             /**< 视频宽度 */
  int height = 480;                            /**< 视频高度 */
  PixelFormat pixelFormat = PixelFormat::YUYV; /**< 像素格式 */
  int frameRate = 15;                          /**< 帧率 */
};

/**
 * @class Capture
 * @brief V4L2视频采集类
 *
 * 用于从V4L2兼容设备(如USB摄像头)采集视频帧
 * 使用MMAP方式进行高效视频捕获
 */
class Capture : public NonCopyable {
 public:
  /**
   * @brief 构造函数
   * @param params 采集参数
   * @throws CaptureException 设备打开失败时抛出
   */
  explicit Capture(const CaptureParams& params);

  /**
   * @brief 析构函数
   */
  ~Capture();

  /**
   * @brief 开始采集流
   * @throws CaptureException 启动失败时抛出
   */
  void start();

  /**
   * @brief 停止采集流
   */
  void stop();

  /**
   * @brief 获取一帧图像
   * @return 包含图像数据的Buffer，超时返回空Buffer
   * @throws CaptureException 发生错误时抛出
   */
  Buffer getData();

  /**
   * @brief 查询亮度控制范围
   * @return 支持时返回ControlRange，否则返回nullopt
   */
  std::optional<ControlRange> queryBrightness() const;

  /**
   * @brief 获取当前亮度值
   * @return 支持时返回亮度值，否则返回nullopt
   */
  std::optional<int> getBrightness() const;

  /**
   * @brief 设置亮度值
   * @param value 新的亮度值
   * @return 成功返回true
   */
  bool setBrightness(int value);

  /**
   * @brief 查询对比度控制范围
   * @return 支持时返回ControlRange，否则返回nullopt
   */
  std::optional<ControlRange> queryContrast() const;

  /**
   * @brief 获取当前对比度值
   * @return 支持时返回对比度值，否则返回nullopt
   */
  std::optional<int> getContrast() const;

  /**
   * @brief 设置对比度值
   * @param value 新的对比度值
   * @return 成功返回true
   */
  bool setContrast(int value);

  /**
   * @brief 查询饱和度控制范围
   * @return 支持时返回ControlRange，否则返回nullopt
   */
  std::optional<ControlRange> querySaturation() const;

  /**
   * @brief 获取当前饱和度值
   * @return 支持时返回饱和度值，否则返回nullopt
   */
  std::optional<int> getSaturation() const;

  /**
   * @brief 设置饱和度值
   * @param value 新的饱和度值
   * @return 成功返回true
   */
  bool setSaturation(int value);

  /**
   * @brief 获取图像大小
   * @return 图像大小(字节)
   */
  int getImageSize() const;

  /**
   * @brief 获取采集参数
   * @return 采集参数引用
   */
  const CaptureParams& getParams() const;

 private:
  class Impl;                   /**< 前向声明实现类 */
  std::unique_ptr<Impl> pImpl_; /**< PIMPL指针 */
};

}  // namespace camera_toolkit
