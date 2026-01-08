/**
 * @file camera_toolkit.cpp
 * @brief Camera Toolkit 命令行工具主程序
 *
 * 提供视频采集、转换、编码、打包和网络传输的命令行接口
 */
#include "camera_toolkit.h"

#include <getopt.h>
#include <linux/videodev2.h>
#include <sys/time.h>
#include <unistd.h>

#include <csignal>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>

namespace {

std::unique_ptr<std::ofstream> outFile; /**< 输出文件 */
volatile sig_atomic_t quit = 0;         /**< 退出标志 */
bool debug = false;                     /**< 调试模式标志 */

/**
 * @brief 信号处理函数
 * @param sig 信号编号(未使用)
 */
void signalHandler(int /*sig*/) { quit = 1; }

/**
 * @brief 显示使用帮助
 */
void displayUsage() {
  std::cout << "Usage: #camtool [options]\n"
            << "-? help\n"
            << "-v version\n"
            << "-d debug on\n"
            << "-s 0/1/3/7/15 set stage:\n"
            << "    0: capture only\n"
            << "    1: capture + convert\n"
            << "    3: capture + convert + encode (default)\n"
            << "    7: capture + convert + encode + pack\n"
            << "    15: capture + convert + encode + pack + network\n"
            << "-i video device (\"/dev/video0\")\n"
            << "-o dump to file (no dump)\n"
            << "-a IP address of stream server (none)\n"
            << "-p port of stream server (none)\n"
            << "-c capture pixel format 0:YUYV, 1:YUV420 (YUYV)\n"
            << "-w width (640)\n"
            << "-h height (480)\n"
            << "-r bitrate kbps (1000)\n"
            << "-f fps (15)\n"
            << "-t chroma interleaved (0)\n"
            << "-g size of group of pictures (12)\n";
}

/**
 * @brief 显示版本信息
 */
void displayVersion() {
  std::cout << "camtool - Camera toolkit\n"
            << "Version: " << camera_toolkit::VERSION << ", built on " << __TIME__ << " " << __DATE__ << "\n\n";
}

/**
 * @brief 将帧类型转换为字符
 * @param type 帧类型
 * @return 对应的字符('S'/'I'/'P'/'B'/'N')
 */
char picTypeToChar(camera_toolkit::PictureType type) {
  switch (type) {
    case camera_toolkit::PictureType::SPS:
    case camera_toolkit::PictureType::PPS:
      return 'S';
    case camera_toolkit::PictureType::I:
      return 'I';
    case camera_toolkit::PictureType::P:
      return 'P';
    case camera_toolkit::PictureType::B:
      return 'B';
    default:
      return 'N';
  }
}

}  // anonymous namespace

/**
 * @brief 主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 成功返回0，失败返回-1
 */
int main(int argc, char* argv[]) {
  // 默认参数
  camera_toolkit::CaptureParams capParams;
  capParams.deviceName = "/dev/video0";
  capParams.width = 640;
  capParams.height = 480;
  capParams.pixelFormat = camera_toolkit::PixelFormat::YUYV;
  capParams.frameRate = 15;

  camera_toolkit::ConvertParams cvtParams;
  cvtParams.inWidth = 640;
  cvtParams.inHeight = 480;
  cvtParams.inPixelFormat = camera_toolkit::PixelFormat::YUYV;
  cvtParams.outWidth = 640;
  cvtParams.outHeight = 480;
  cvtParams.outPixelFormat = camera_toolkit::PixelFormat::YUV420;

  camera_toolkit::EncoderParams encParams;
  encParams.srcWidth = 640;
  encParams.srcHeight = 480;
  encParams.encWidth = 640;
  encParams.encHeight = 480;
  encParams.chromaInterleave = false;
  encParams.fps = 15;
  encParams.gop = 12;
  encParams.bitrate = 1000;

  camera_toolkit::RTPPackerParams pacParams;
  pacParams.maxPacketLength = 1400;
  pacParams.ssrc = 1234;

  camera_toolkit::NetworkParams netParams;
  netParams.type = camera_toolkit::NetworkType::UDP;
  netParams.serverIP = "";
  netParams.serverPort = -1;

  camera_toolkit::TimestampParams tmsParams;
  tmsParams.startX = 10;
  tmsParams.startY = 10;
  tmsParams.videoWidth = 640;
  tmsParams.factor = 0;

  int stage = 0b00000011;
  std::string outFilename;

  // 解析命令行选项
  static const char* optString = "?vdi:o:a:p:w:h:r:f:t:g:s:c:";
  int opt;

  while ((opt = getopt(argc, argv, optString)) != -1) {
    switch (opt) {
      case '?':
        displayUsage();
        return 0;
      case 'v':
        displayVersion();
        return 0;
      case 'd':
        debug = true;
        break;
      case 's':
        stage = std::stoi(optarg);
        break;
      case 'i':
        capParams.deviceName = optarg;
        break;
      case 'o':
        outFilename = optarg;
        break;
      case 'a':
        netParams.serverIP = optarg;
        break;
      case 'p':
        netParams.serverPort = std::stoi(optarg);
        break;
      case 'c': {
        int fmt = std::stoi(optarg);
        if (fmt == 1) {
          capParams.pixelFormat = camera_toolkit::PixelFormat::YUV420;
        } else {
          capParams.pixelFormat = camera_toolkit::PixelFormat::YUYV;
        }
        break;
      }
      case 'w': {
        int width = std::stoi(optarg);
        capParams.width = cvtParams.inWidth = cvtParams.outWidth = encParams.srcWidth = encParams.encWidth =
            tmsParams.videoWidth = width;
        break;
      }
      case 'h': {
        int height = std::stoi(optarg);
        capParams.height = cvtParams.inHeight = cvtParams.outHeight = encParams.srcHeight = encParams.encHeight =
            height;
        break;
      }
      case 'r':
        encParams.bitrate = std::stoi(optarg);
        break;
      case 'f':
        capParams.frameRate = encParams.fps = std::stoi(optarg);
        break;
      case 't':
        encParams.chromaInterleave = (std::stoi(optarg) != 0);
        break;
      case 'g':
        encParams.gop = std::stoi(optarg);
        break;
      default:
        std::cerr << "Unknown option: " << optarg << std::endl;
        displayUsage();
        return -1;
    }
  }

  // 打开输出文件(如果指定)
  if (!outFilename.empty()) {
    outFile = std::make_unique<std::ofstream>(outFilename, std::ios::binary);
    if (!outFile->is_open()) {
      std::cerr << "--- Failed to open output file: " << outFilename << std::endl;
      return -1;
    }
  }

  // 设置信号处理
  signal(SIGINT, signalHandler);

  // 打印版本信息
  displayVersion();

  try {
    // 创建组件
    auto capture = std::make_unique<camera_toolkit::Capture>(capParams);

    std::unique_ptr<camera_toolkit::Convert> convert;
    std::unique_ptr<camera_toolkit::Encoder> encoder;
    std::unique_ptr<camera_toolkit::RTPPacker> packer;
    std::unique_ptr<camera_toolkit::Network> network;
    std::unique_ptr<camera_toolkit::Timestamp> timestamp;

    if ((stage & 0b00000001) != 0) {
      cvtParams.inPixelFormat = capParams.pixelFormat;
      convert = std::make_unique<camera_toolkit::Convert>(cvtParams);
    }

    if ((stage & 0b00000010) != 0) {
      encoder = std::make_unique<camera_toolkit::Encoder>(encParams);
    }

    if ((stage & 0b00000100) != 0) {
      packer = std::make_unique<camera_toolkit::RTPPacker>(pacParams);
    }

    if ((stage & 0b00001000) != 0) {
      if (netParams.serverIP.empty() || netParams.serverPort == -1) {
        std::cerr << "--- Server IP and port must be specified when using network" << std::endl;
        return -1;
      }
      network = std::make_unique<camera_toolkit::Network>(netParams);
    }

    timestamp = std::make_unique<camera_toolkit::Timestamp>(tmsParams);

    // 开始采集循环
    capture->start();

    struct timeval currentTime, lastTime;
    unsigned long fpsCounter = 0;
    gettimeofday(&lastTime, nullptr);

    while (!quit) {
      // FPS计算
      if (debug) {
        gettimeofday(&currentTime, nullptr);
        int sec = currentTime.tv_sec - lastTime.tv_sec;
        int usec = currentTime.tv_usec - lastTime.tv_usec;
        if (usec < 0) {
          sec--;
          usec += 1000000;
        }
        double statTime = (sec * 1000000) + usec;

        if (statTime >= 1000000) {
          std::cout << "\n*** FPS: " << fpsCounter << std::endl;
          fpsCounter = 0;
          lastTime = currentTime;
        }
        fpsCounter++;
      }

      // 采集
      camera_toolkit::Buffer capBuf = capture->getData();
      if (capBuf.empty()) {
        usleep(10000);
        continue;
      }

      if (debug) std::cout << '.' << std::flush;

      if ((stage & 0b00000001) == 0) {
        // 仅采集
        if (outFile) {
          outFile->write(static_cast<char*>(capBuf.data), capBuf.size);
        }
        continue;
      }

      // 转换
      camera_toolkit::Buffer cvtBuf;
      if (capParams.pixelFormat == camera_toolkit::PixelFormat::YUV420) {
        cvtBuf = capBuf;  // 无需转换
      } else {
        cvtBuf = convert->convert(capBuf);
        if (cvtBuf.empty()) {
          std::cerr << "!!! No convert data" << std::endl;
          continue;
        }
      }

      if (debug) std::cout << '-' << std::flush;

      // 绘制时间戳
      timestamp->draw(static_cast<uint8_t*>(cvtBuf.data));

      if ((stage & 0b00000010) == 0) {
        // 无编码
        if (outFile) {
          outFile->write(static_cast<char*>(cvtBuf.data), cvtBuf.size);
        }
        continue;
      }

      // 获取头信息(SPS/PPS)
      while (auto header = encoder->getHeaders()) {
        if (debug) std::cout << 'S' << std::flush;

        if ((stage & 0b00000100) == 0) {
          if (outFile) {
            outFile->write(static_cast<char*>(header->buffer.data), header->buffer.size);
          }
          continue;
        }

        // 打包头信息
        packer->put(header->buffer);
        while (auto packet = packer->get()) {
          if (debug) std::cout << '#' << std::flush;

          if ((stage & 0b00001000) == 0) {
            if (outFile) {
              outFile->write(static_cast<char*>(packet->data), packet->size);
            }
            continue;
          }

          // 发送
          int ret = network->send(*packet);
          if (ret != packet->size) {
            std::cerr << "!!! send failed, size: " << packet->size << ", err: " << strerror(errno) << std::endl;
          }
          if (debug) std::cout << '>' << std::flush;
        }
      }

      // 编码
      auto encoded = encoder->encode(cvtBuf);
      if (encoded.empty()) {
        std::cerr << "!!! No encode data" << std::endl;
        continue;
      }

      if (debug) std::cout << picTypeToChar(encoded.type) << std::flush;

      if ((stage & 0b00000100) == 0) {
        // 无打包
        if (outFile) {
          outFile->write(static_cast<char*>(encoded.buffer.data), encoded.buffer.size);
        }
        continue;
      }

      // 打包
      packer->put(encoded.buffer);
      while (auto packet = packer->get()) {
        if (debug) std::cout << '#' << std::flush;

        if ((stage & 0b00001000) == 0) {
          // 无网络
          if (outFile) {
            outFile->write(static_cast<char*>(packet->data), packet->size);
          }
          continue;
        }

        // 网络发送
        int ret = network->send(*packet);
        if (ret != packet->size) {
          std::cerr << "!!! send failed, size: " << packet->size << ", err: " << strerror(errno) << std::endl;
        }
        if (debug) std::cout << '>' << std::flush;
      }
    }

    capture->stop();

  } catch (const camera_toolkit::camera_toolkitException& e) {
    std::cerr << "--- Error: " << e.what() << std::endl;
    return -1;
  }

  if (outFile) {
    outFile->close();
  }

  return 0;
}
