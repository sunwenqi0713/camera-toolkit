/**
 * @file network.h
 * @brief 网络通信类定义
 *
 * 通过UDP或TCP连接发送和接收数据
 */
#pragma once

#include <memory>
#include <string>

#include "common.h"

namespace camera_toolkit {

/**
 * @brief 网络配置参数结构体
 */
struct NetworkParams {
  NetworkType type = NetworkType::UDP; /**< 协议类型(UDP或TCP) */
  std::string serverIP;                /**< 服务器IP地址 */
  int serverPort = 0;                  /**< 服务器端口 */
};

/**
 * @class Network
 * @brief 网络通信类
 *
 * 通过UDP或TCP连接发送和接收数据
 */
class Network : public NonCopyable {
 public:
  /**
   * @brief 构造函数
   * @param params 网络参数
   * @throws NetworkException 连接失败时抛出
   */
  explicit Network(const NetworkParams& params);

  /**
   * @brief 析构函数
   */
  ~Network();

  /**
   * @brief 发送数据
   * @param data 要发送的数据缓冲区
   * @param size 数据大小(字节)
   * @return 发送的字节数，错误返回-1
   */
  int send(const void* data, int size);

  /**
   * @brief 发送缓冲区
   * @param buffer 要发送的Buffer
   * @return 发送的字节数，错误返回-1
   */
  int send(const Buffer& buffer);

  /**
   * @brief 接收数据
   * @param data 接收缓冲区
   * @param size 最大接收字节数
   * @return 接收的字节数，错误返回-1
   */
  int receive(void* data, int size);

  /**
   * @brief 检查连接是否有效
   * @return 已连接返回true
   */
  bool isConnected() const;

  /**
   * @brief 获取网络参数
   * @return 网络参数引用
   */
  const NetworkParams& getParams() const;

 private:
  class Impl;                   /**< 前向声明实现类 */
  std::unique_ptr<Impl> pImpl_; /**< PIMPL指针 */
};

}  // namespace camera_toolkit
