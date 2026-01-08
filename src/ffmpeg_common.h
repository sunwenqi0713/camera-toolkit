/**
 * @file ffmpeg_common.h
 * @brief FFmpeg公共工具函数
 *
 * 包含Camera Toolkit与FFmpeg像素格式转换的工具函数
 */
#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <linux/videodev2.h>

#include "camera_toolkit/common.h"

namespace camera_toolkit {

/**
 * @brief 将camera_toolkit像素格式转换为FFmpeg像素格式
 * @param format camera_toolkit像素格式
 * @return 对应的FFmpeg像素格式，无法转换时返回AV_PIX_FMT_NONE
 */
inline AVPixelFormat toAVPixelFormat(PixelFormat format) {
  switch (format) {
    case PixelFormat::YUV420:
      return AV_PIX_FMT_YUV420P;
    case PixelFormat::YUYV:
      return AV_PIX_FMT_YUYV422;
    case PixelFormat::RGB565:
      return AV_PIX_FMT_RGB565LE;
    case PixelFormat::RGB24:
      return AV_PIX_FMT_RGB24;
    default:
      return AV_PIX_FMT_NONE;
  }
}

/**
 * @brief 将V4L2像素格式转换为FFmpeg像素格式
 * @param v4lFormat V4L2像素格式
 * @return 对应的FFmpeg像素格式，无法转换时返回AV_PIX_FMT_NONE
 */
inline AVPixelFormat v4l2ToAVPixelFormat(uint32_t v4lFormat) {
  switch (v4lFormat) {
    case V4L2_PIX_FMT_YUV420:
      return AV_PIX_FMT_YUV420P;
    case V4L2_PIX_FMT_YUYV:
      return AV_PIX_FMT_YUYV422;
    case V4L2_PIX_FMT_RGB565:
      return AV_PIX_FMT_RGB565LE;
    case V4L2_PIX_FMT_RGB24:
      return AV_PIX_FMT_RGB24;
    default:
      return AV_PIX_FMT_NONE;
  }
}

}  // namespace camera_toolkit
