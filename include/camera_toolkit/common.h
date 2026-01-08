/**
 * @file common.h
 * @brief Camera Toolkit 公共定义头文件
 *
 * 包含枚举类型、异常类、通用结构体等公共定义
 */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace camera_toolkit {

/**
 * @brief H264帧类型枚举
 */
enum class PictureType {
  None = -1, /**< 无类型 */
  SPS,       /**< 序列参数集 */
  PPS,       /**< 图像参数集 */
  I,         /**< I帧(关键帧) */
  P,         /**< P帧(预测帧) */
  B          /**< B帧(双向预测帧) */
};

/**
 * @brief 网络协议类型枚举
 */
enum class NetworkType {
  UDP = 0, /**< UDP协议 */
  TCP      /**< TCP协议 */
};

/**
 * @brief 像素格式枚举 (兼容V4L2)
 */
enum class PixelFormat : uint32_t {
  YUYV = 0x56595559,   /**< V4L2_PIX_FMT_YUYV */
  YUV420 = 0x32315559, /**< V4L2_PIX_FMT_YUV420 */
  RGB565 = 0x50424752, /**< V4L2_PIX_FMT_RGB565 */
  RGB24 = 0x33424752   /**< V4L2_PIX_FMT_RGB24 */
};

/**
 * @brief 图像数据缓冲区结构体
 */
struct Buffer {
  void* data = nullptr; /**< 数据指针 */
  int size = 0;         /**< 数据大小(字节) */

  /** @brief 默认构造函数 */
  Buffer() = default;

  /**
   * @brief 构造函数
   * @param d 数据指针
   * @param s 数据大小
   */
  Buffer(void* d, int s) : data(d), size(s) {}

  /**
   * @brief 检查缓冲区是否为空
   * @return 为空返回true
   */
  bool empty() const { return data == nullptr || size <= 0; }
};

/**
 * @brief camera_toolkit异常基类
 */
class camera_toolkitException : public std::runtime_error {
 public:
  /**
   * @brief 构造函数
   * @param message 错误消息
   */
  explicit camera_toolkitException(const std::string& message) : std::runtime_error(message) {}
};

/**
 * @brief 采集异常类
 */
class CaptureException : public camera_toolkitException {
 public:
  /**
   * @brief 构造函数
   * @param message 错误消息
   */
  explicit CaptureException(const std::string& message) : camera_toolkitException("Capture error: " + message) {}
};

/**
 * @brief 转换异常类
 */
class ConvertException : public camera_toolkitException {
 public:
  /**
   * @brief 构造函数
   * @param message 错误消息
   */
  explicit ConvertException(const std::string& message) : camera_toolkitException("Convert error: " + message) {}
};

/**
 * @brief 编码异常类
 */
class EncodeException : public camera_toolkitException {
 public:
  /**
   * @brief 构造函数
   * @param message 错误消息
   */
  explicit EncodeException(const std::string& message) : camera_toolkitException("Encode error: " + message) {}
};

/**
 * @brief 网络异常类
 */
class NetworkException : public camera_toolkitException {
 public:
  /**
   * @brief 构造函数
   * @param message 错误消息
   */
  explicit NetworkException(const std::string& message) : camera_toolkitException("Network error: " + message) {}
};

/**
 * @brief 打包异常类
 */
class PackException : public camera_toolkitException {
 public:
  /**
   * @brief 构造函数
   * @param message 错误消息
   */
  explicit PackException(const std::string& message) : camera_toolkitException("Pack error: " + message) {}
};

/**
 * @brief 不可复制基类
 *
 * 继承此类以禁用复制构造和复制赋值
 */
class NonCopyable {
 protected:
  NonCopyable() = default;
  ~NonCopyable() = default;

  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;
  NonCopyable(NonCopyable&&) = default;
  NonCopyable& operator=(NonCopyable&&) = default;
};

/**
 * @brief 图像控制参数范围结构体
 */
struct ControlRange {
  int min = 0;  /**< 最小值 */
  int max = 0;  /**< 最大值 */
  int step = 0; /**< 步进值 */
};

}  // namespace camera_toolkit
