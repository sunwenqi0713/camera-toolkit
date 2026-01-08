/**
 * @file network.cpp
 * @brief 网络通信类实现
 */
#include "camera_toolkit/network.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

namespace camera_toolkit {

/**
 * @brief Network类的PIMPL实现
 */
class Network::Impl {
 public:
  /**
   * @brief 构造函数
   * @param params 网络参数
   * @throws NetworkException 连接失败时抛出
   */
  explicit Impl(const NetworkParams& params) : params_(params) {
    // 创建套接字
    if (params_.type == NetworkType::TCP) {
      socketFd_ = socket(AF_INET, SOCK_STREAM, 0);
    } else {
      socketFd_ = socket(AF_INET, SOCK_DGRAM, 0);
    }

    if (socketFd_ < 0) {
      throw NetworkException("Failed to create socket");
    }

    // 设置服务器地址
    std::memset(&serverAddr_, 0, sizeof(serverAddr_));
    serverAddr_.sin_family = AF_INET;
    serverAddr_.sin_port = htons(params_.serverPort);
    serverAddr_.sin_addr.s_addr = inet_addr(params_.serverIP.c_str());

    // 连接
    int ret = connect(socketFd_, reinterpret_cast<struct sockaddr*>(&serverAddr_), sizeof(serverAddr_));
    if (ret < 0) {
      close(socketFd_);
      socketFd_ = -1;
      throw NetworkException("Failed to connect to server " + params_.serverIP + ":" +
                             std::to_string(params_.serverPort));
    }

    connected_ = true;
    std::cout << "+++ Network opened (" << (params_.type == NetworkType::TCP ? "TCP" : "UDP") << " -> "
              << params_.serverIP << ":" << params_.serverPort << ")" << std::endl;
  }

  /**
   * @brief 析构函数
   */
  ~Impl() {
    if (socketFd_ >= 0) {
      close(socketFd_);
      socketFd_ = -1;
    }
    std::cout << "+++ Network closed" << std::endl;
  }

  /**
   * @brief 发送数据
   * @param data 要发送的数据缓冲区
   * @param size 数据大小(字节)
   * @return 发送的字节数，错误返回-1
   */
  int send(const void* data, int size) { return ::send(socketFd_, data, size, 0); }

  /**
   * @brief 接收数据
   * @param data 接收缓冲区
   * @param size 最大接收字节数
   * @return 接收的字节数，错误返回-1
   */
  int receive(void* data, int size) { return ::recv(socketFd_, data, size, 0); }

  /**
   * @brief 检查连接是否有效
   * @return 已连接返回true
   */
  bool isConnected() const { return connected_; }

  /**
   * @brief 获取网络参数
   * @return 网络参数引用
   */
  const NetworkParams& getParams() const { return params_; }

 private:
  NetworkParams params_;            /**< 网络参数 */
  int socketFd_ = -1;               /**< 套接字文件描述符 */
  struct sockaddr_in serverAddr_{}; /**< 服务器地址 */
  bool connected_ = false;          /**< 连接状态 */
};

// ============================================================================
// 公共接口实现
// ============================================================================

Network::Network(const NetworkParams& params) : pImpl_(std::make_unique<Impl>(params)) {}

Network::~Network() = default;

int Network::send(const void* data, int size) { return pImpl_->send(data, size); }

int Network::send(const Buffer& buffer) { return pImpl_->send(buffer.data, buffer.size); }

int Network::receive(void* data, int size) { return pImpl_->receive(data, size); }

bool Network::isConnected() const { return pImpl_->isConnected(); }

const NetworkParams& Network::getParams() const { return pImpl_->getParams(); }

}  // namespace camera_toolkit
