/**
 * @file capture.cpp
 * @brief V4L2视频采集类实现
 */
#include "camera_toolkit/capture.h"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <vector>

namespace camera_toolkit {

namespace {

/**
 * @brief 带中断重试的ioctl封装
 * @param fd 文件描述符
 * @param request ioctl请求码
 * @param arg 参数指针
 * @return ioctl返回值
 */
int xioctl(int fd, unsigned long request, void* arg) {
  int r;
  do {
    r = ioctl(fd, request, arg);
  } while (r == -1 && errno == EINTR);
  return r;
}

/**
 * @brief 将camera_toolkit像素格式转换为V4L2格式
 * @param format camera_toolkit像素格式
 * @return V4L2像素格式
 */
uint32_t toV4L2Format(PixelFormat format) {
  switch (format) {
    case PixelFormat::YUYV:
      return V4L2_PIX_FMT_YUYV;
    case PixelFormat::YUV420:
      return V4L2_PIX_FMT_YUV420;
    case PixelFormat::RGB565:
      return V4L2_PIX_FMT_RGB565;
    case PixelFormat::RGB24:
      return V4L2_PIX_FMT_RGB24;
    default:
      return V4L2_PIX_FMT_YUYV;
  }
}

}  // anonymous namespace

/**
 * @brief 缓冲区信息结构体
 */
struct BufferInfo {
  void* start = nullptr; /**< 缓冲区起始地址 */
  size_t length = 0;     /**< 缓冲区长度 */
};

/**
 * @brief Capture类的PIMPL实现
 */
class Capture::Impl {
 public:
  /**
   * @brief 构造函数
   * @param params 采集参数
   * @throws CaptureException 设备打开或初始化失败时抛出
   */
  explicit Impl(const CaptureParams& params) : params_(params) {
    // 检查设备
    struct stat st;
    if (stat(params_.deviceName.c_str(), &st) == -1) {
      throw CaptureException("Cannot identify video device " + params_.deviceName + ": " + std::strerror(errno));
    }

    if (!S_ISCHR(st.st_mode)) {
      throw CaptureException(params_.deviceName + " is not a char device");
    }

    // 打开设备
    fd_ = open(params_.deviceName.c_str(), O_RDWR | O_NONBLOCK, 0);
    if (fd_ == -1) {
      throw CaptureException("Cannot open device " + params_.deviceName + ": " + std::strerror(errno));
    }

    std::cout << "+++ Capture opened" << std::endl;

    initDevice();
    std::cout << "+++ Capture initialized" << std::endl;
  }

  /**
   * @brief 析构函数
   */
  ~Impl() {
    uninitDevice();

    if (fd_ != -1) {
      close(fd_);
      fd_ = -1;
    }

    std::cout << "+++ Capture closed" << std::endl;
  }

  /**
   * @brief 开始采集流
   * @throws CaptureException 启动失败时抛出
   */
  void start() {
    for (unsigned int i = 0; i < buffers_.size(); ++i) {
      struct v4l2_buffer buf{};
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      buf.index = i;

      if (xioctl(fd_, VIDIOC_QBUF, &buf) == -1) {
        throw CaptureException("VIDIOC_QBUF failed for index " + std::to_string(i));
      }
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd_, VIDIOC_STREAMON, &type) == -1) {
      throw CaptureException("VIDIOC_STREAMON failed");
    }

    v4lBufPut_ = true;
    std::cout << "+++ Capture started" << std::endl;
  }

  /**
   * @brief 停止采集流
   */
  void stop() {
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(fd_, VIDIOC_STREAMOFF, &type);
    std::cout << "+++ Capture stopped" << std::endl;
  }

  /**
   * @brief 获取一帧图像
   * @return 包含图像数据的Buffer，超时返回空Buffer
   * @throws CaptureException 发生错误时抛出
   */
  Buffer getData() {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);

    struct timeval tv{};
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    int ret = select(fd_ + 1, &fds, nullptr, nullptr, &tv);
    if (ret == -1) {
      throw CaptureException("select failed: " + std::string(std::strerror(errno)));
    }

    if (ret == 0) {
      // 超时
      return Buffer();
    }

    // 如果v4l缓冲区未入队，则入队
    if (!v4lBufPut_) {
      if (xioctl(fd_, VIDIOC_QBUF, &v4lBuf_) == -1) {
        throw CaptureException("VIDIOC_QBUF failed");
      }
      v4lBufPut_ = true;
    }

    // 从队列获取缓冲区
    std::memset(&v4lBuf_, 0, sizeof(v4lBuf_));
    v4lBuf_.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4lBuf_.memory = V4L2_MEMORY_MMAP;

    if (xioctl(fd_, VIDIOC_DQBUF, &v4lBuf_) == -1) {
      if (errno == EAGAIN) {
        return Buffer();  // 重试
      }
      throw CaptureException("VIDIOC_DQBUF failed");
    }

    assert(v4lBuf_.index < buffers_.size());

    v4lBufPut_ = false;
    imageCounter_++;

    return Buffer(buffers_[v4lBuf_.index].start, imageSize_);
  }

  /**
   * @brief 查询控制参数范围
   * @param controlId V4L2控制ID
   * @return 支持时返回ControlRange，否则返回nullopt
   */
  std::optional<ControlRange> queryControl(uint32_t controlId) const {
    struct v4l2_queryctrl qctrl{};
    qctrl.id = controlId;

    if (ioctl(fd_, VIDIOC_QUERYCTRL, &qctrl) < 0) {
      return std::nullopt;
    }

    ControlRange range;
    range.min = qctrl.minimum;
    range.max = qctrl.maximum;
    range.step = qctrl.step;
    return range;
  }

  /**
   * @brief 获取控制参数值
   * @param controlId V4L2控制ID
   * @return 支持时返回控制值，否则返回nullopt
   */
  std::optional<int> getControl(uint32_t controlId) const {
    struct v4l2_control ctrl{};
    ctrl.id = controlId;

    if (ioctl(fd_, VIDIOC_G_CTRL, &ctrl) < 0) {
      return std::nullopt;
    }

    return ctrl.value;
  }

  /**
   * @brief 设置控制参数值
   * @param controlId V4L2控制ID
   * @param value 控制值
   * @return 成功返回true
   */
  bool setControl(uint32_t controlId, int value) {
    struct v4l2_control ctrl{};
    ctrl.id = controlId;
    ctrl.value = value;
    return ioctl(fd_, VIDIOC_S_CTRL, &ctrl) >= 0;
  }

  /**
   * @brief 获取图像大小
   * @return 图像大小(字节)
   */
  int getImageSize() const { return imageSize_; }

  /**
   * @brief 获取采集参数
   * @return 采集参数引用
   */
  const CaptureParams& getParams() const { return params_; }

 private:
  /**
   * @brief 初始化设备
   * @throws CaptureException 初始化失败时抛出
   */
  void initDevice() {
    // 查询设备能力
    struct v4l2_capability cap{};
    if (xioctl(fd_, VIDIOC_QUERYCAP, &cap) == -1) {
      throw CaptureException(params_.deviceName + " is not a V4L2 device");
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
      throw CaptureException(params_.deviceName + " is not a video capture device");
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
      throw CaptureException(params_.deviceName + " does not support streaming");
    }

    // 设置裁剪(可选)
    struct v4l2_cropcap cropcap{};
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd_, VIDIOC_CROPCAP, &cropcap) == 0) {
      struct v4l2_crop crop{};
      crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      crop.c = cropcap.defrect;

      if (xioctl(fd_, VIDIOC_S_CROP, &crop) == 0) {
        std::cout << "+++ Set crop to (" << crop.c.left << ", " << crop.c.top << ", " << crop.c.width << ", "
                  << crop.c.height << ")" << std::endl;
      }
    }

    // 设置格式
    struct v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = params_.width;
    fmt.fmt.pix.height = params_.height;
    fmt.fmt.pix.pixelformat = toV4L2Format(params_.pixelFormat);
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (xioctl(fd_, VIDIOC_S_FMT, &fmt) == -1) {
      throw CaptureException("VIDIOC_S_FMT failed");
    }

    imageSize_ = fmt.fmt.pix.sizeimage;

    if (static_cast<int>(fmt.fmt.pix.width) != params_.width ||
        static_cast<int>(fmt.fmt.pix.height) != params_.height) {
      throw CaptureException("Set resolution to (" + std::to_string(params_.width) + ", " +
                             std::to_string(params_.height) + ") failed, camera supports (" +
                             std::to_string(fmt.fmt.pix.width) + ", " + std::to_string(fmt.fmt.pix.height) + ")");
    }

    // 设置流参数
    struct v4l2_streamparm sparam{};
    sparam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    sparam.parm.capture.capturemode = V4L2_MODE_HIGHQUALITY;
    sparam.parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
    sparam.parm.capture.timeperframe.denominator = params_.frameRate;
    sparam.parm.capture.timeperframe.numerator = 1;

    if (xioctl(fd_, VIDIOC_S_PARM, &sparam) == -1) {
      std::cerr << "!!! Set capture params failed" << std::endl;
    }

    // 设置输入
    int input = 1;
    if (ioctl(fd_, VIDIOC_S_INPUT, &input) < 0) {
      // 非关键错误
    }

    initMmap();
  }

  /**
   * @brief 初始化内存映射
   * @throws CaptureException 内存映射失败时抛出
   */
  void initMmap() {
    struct v4l2_requestbuffers req{};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(fd_, VIDIOC_REQBUFS, &req) == -1) {
      throw CaptureException(params_.deviceName + " does not support memory mapping");
    }

    if (req.count < 2) {
      throw CaptureException("Insufficient buffer memory on " + params_.deviceName);
    }

    buffers_.resize(req.count);

    for (unsigned int i = 0; i < req.count; ++i) {
      struct v4l2_buffer buf{};
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      buf.index = i;

      if (xioctl(fd_, VIDIOC_QUERYBUF, &buf) == -1) {
        throw CaptureException("VIDIOC_QUERYBUF failed for index " + std::to_string(i));
      }

      buffers_[i].length = buf.length;
      buffers_[i].start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, buf.m.offset);

      if (buffers_[i].start == MAP_FAILED) {
        throw CaptureException("Memory map failed for index " + std::to_string(i));
      }
    }

    std::cout << "+++ Device initialized with " << buffers_.size() << " buffers" << std::endl;
  }

  /**
   * @brief 反初始化设备
   */
  void uninitDevice() {
    for (auto& buffer : buffers_) {
      if (buffer.start && buffer.start != MAP_FAILED) {
        munmap(buffer.start, buffer.length);
      }
    }
    buffers_.clear();
    std::cout << "+++ Device uninitialized" << std::endl;
  }

  CaptureParams params_;            /**< 采集参数 */
  int fd_ = -1;                     /**< 文件描述符 */
  int imageSize_ = 0;               /**< 图像大小 */
  std::vector<BufferInfo> buffers_; /**< 缓冲区列表 */
  struct v4l2_buffer v4lBuf_{};     /**< V4L2缓冲区 */
  bool v4lBufPut_ = true;           /**< 缓冲区是否已入队 */
  unsigned long imageCounter_ = 0;  /**< 图像计数器 */
};

// ============================================================================
// 公共接口实现
// ============================================================================

Capture::Capture(const CaptureParams& params) : pImpl_(std::make_unique<Impl>(params)) {}

Capture::~Capture() = default;

void Capture::start() { pImpl_->start(); }

void Capture::stop() { pImpl_->stop(); }

Buffer Capture::getData() { return pImpl_->getData(); }

std::optional<ControlRange> Capture::queryBrightness() const { return pImpl_->queryControl(V4L2_CID_BRIGHTNESS); }

std::optional<int> Capture::getBrightness() const { return pImpl_->getControl(V4L2_CID_BRIGHTNESS); }

bool Capture::setBrightness(int value) { return pImpl_->setControl(V4L2_CID_BRIGHTNESS, value); }

std::optional<ControlRange> Capture::queryContrast() const { return pImpl_->queryControl(V4L2_CID_CONTRAST); }

std::optional<int> Capture::getContrast() const { return pImpl_->getControl(V4L2_CID_CONTRAST); }

bool Capture::setContrast(int value) { return pImpl_->setControl(V4L2_CID_CONTRAST, value); }

std::optional<ControlRange> Capture::querySaturation() const { return pImpl_->queryControl(V4L2_CID_SATURATION); }

std::optional<int> Capture::getSaturation() const { return pImpl_->getControl(V4L2_CID_SATURATION); }

bool Capture::setSaturation(int value) { return pImpl_->setControl(V4L2_CID_SATURATION, value); }

int Capture::getImageSize() const { return pImpl_->getImageSize(); }

const CaptureParams& Capture::getParams() const { return pImpl_->getParams(); }

}  // namespace camera_toolkit
