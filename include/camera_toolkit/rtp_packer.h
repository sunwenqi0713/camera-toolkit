/**
 * @file rtp_packer.h
 * @brief RTP打包器类定义
 *
 * 将H264 NAL单元打包成RTP包用于流媒体传输
 */
#pragma once

#include <memory>
#include <optional>

#include "common.h"

namespace camera_toolkit {

/**
 * @brief RTP打包器配置参数结构体
 */
struct RTPPackerParams {
  int maxPacketLength = 1400; /**< 最大包长度(应小于MTU 1500) */
  uint32_t ssrc = 1234;       /**< 同步源标识符 */
};

/**
 * @class RTPPacker
 * @brief RTP打包器类
 *
 * 将H264 NAL单元打包成RTP包用于流媒体传输
 * 支持FU-A分片模式处理大型NAL单元
 */
class RTPPacker : public NonCopyable {
 public:
  /**
   * @brief 构造函数
   * @param params 打包器参数
   * @throws PackException 初始化失败时抛出
   */
  explicit RTPPacker(const RTPPackerParams& params);

  /**
   * @brief 析构函数
   */
  ~RTPPacker();

  /**
   * @brief 放入待打包的NAL单元
   * @param input 包含一个或多个NAL单元的缓冲区
   */
  void put(const Buffer& input);

  /**
   * @brief 获取下一个RTP包
   * @return 包含RTP包的Buffer，无更多包时返回nullopt
   */
  std::optional<Buffer> get();

  /**
   * @brief 获取打包器参数
   * @return 打包器参数引用
   */
  const RTPPackerParams& getParams() const;

 private:
  class Impl;                   /**< 前向声明实现类 */
  std::unique_ptr<Impl> pImpl_; /**< PIMPL指针 */
};

}  // namespace camera_toolkit
