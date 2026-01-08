# Camera Toolkit

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)](https://www.linux.org/)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

Camera Toolkit 是一个用于视频采集和流媒体处理的 C++17 库，提供从摄像头采集到网络传输的完整处理流水线。

> ⚠️ **平台要求**: 仅支持 Linux 系统（使用 V4L2 和 POSIX API）

## 功能特性

- **视频采集** - 基于 V4L2 的高效视频捕获（MMAP 模式）
- **色彩转换** - 使用 FFmpeg swscale 进行像素格式和分辨率转换
- **H.264 编码** - 基于 FFmpeg libavcodec 的低延迟编码
- **RTP 打包** - 支持 FU-A 分片的 RTP 封装
- **网络传输** - UDP/TCP 数据发送
- **时间戳叠加** - 在视频帧上绘制时间戳

## 模块架构

```
┌─────────┐   ┌─────────┐   ┌─────────┐   ┌───────────┐   ┌─────────┐
│ Capture │ → │ Convert │ → │ Encoder │ → │ RTPPacker │ → │ Network │
└─────────┘   └─────────┘   └─────────┘   └───────────┘   └─────────┘
   V4L2         swscale       x264/h264       RTP/FU-A      UDP/TCP
```

## 系统要求

- **操作系统**: Linux (内核 3.0+)
- **编译器**: GCC 7+ 或 Clang 5+ (需支持 C++17)
- **构建工具**: CMake 3.16+

## 依赖安装

Ubuntu/Debian/Raspberry Pi OS：

```bash
sudo apt-get install cmake build-essential pkg-config \
    libavcodec-dev libswscale-dev libavutil-dev
```

## 构建与安装

### 从源码构建

克隆仓库并使用 CMake 构建：

```bash
git clone https://github.com/sunwenqi0713/camera-toolkit.git
cd camera-toolkit
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

### CMake 构建选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `DEBUG` | 启用调试模式 | `OFF` |
| `BUILD_SHARED_LIBS` | 构建动态库 | `ON` |
| `BUILD_TOOL` | 构建命令行工具 | `ON` |

## 快速开始

### 库使用示例

```cpp
#include <camera_toolkit.h>
#include <iostream>

int main() {
    using namespace camera_toolkit;
    
    try {
        // 配置采集参数
        CaptureParams capParams;
        capParams.deviceName = "/dev/video0";
        capParams.width = 640;
        capParams.height = 480;
        capParams.pixelFormat = PixelFormat::YUYV;
        capParams.frameRate = 15;
        
        // 配置转换参数
        ConvertParams cvtParams;
        cvtParams.inWidth = 640;
        cvtParams.inHeight = 480;
        cvtParams.inPixelFormat = PixelFormat::YUYV;
        cvtParams.outWidth = 640;
        cvtParams.outHeight = 480;
        cvtParams.outPixelFormat = PixelFormat::YUV420;
        
        // 配置编码参数
        EncoderParams encParams;
        encParams.srcWidth = 640;
        encParams.srcHeight = 480;
        encParams.fps = 15;
        encParams.bitrate = 1000;  // kbps
        encParams.gop = 12;
        
        // 创建各模块
        Capture capture(capParams);
        Convert convert(cvtParams);
        Encoder encoder(encParams);
        
        // 开始采集
        capture.start();
        
        bool running = true;
        while (running) {
            // 采集一帧
            Buffer capBuf = capture.getData();
            if (capBuf.empty()) continue;
            
            // 色彩转换
            Buffer cvtBuf = convert.convert(capBuf);
            
            // 编码
            EncodedFrame encoded = encoder.encode(cvtBuf);
            if (!encoded.empty()) {
                // 使用编码后的数据...
                std::cout << "Encoded frame: " << encoded.buffer.size << " bytes\n";
            }
        }
        
        capture.stop();
        
    } catch (const CaptureException& e) {
        std::cerr << "Capture error: " << e.what() << std::endl;
        return 1;
    } catch (const ConvertException& e) {
        std::cerr << "Convert error: " << e.what() << std::endl;
        return 1;
    } catch (const EncodeException& e) {
        std::cerr << "Encode error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

### 编译链接

```bash
g++ -std=c++17 main.cpp -o main -lcamera_toolkit
```

或使用 CMake:

```cmake
find_package(camera_toolkit REQUIRED)
target_link_libraries(your_target PRIVATE camera_toolkit::camera_toolkit)
```

## 命令行工具

安装后可使用 `camtool` 命令行工具进行快速测试。

### 基本用法

```bash
# 显示帮助
camtool -?

# 显示版本
camtool -v

# 采集 + 转换 + 编码，保存到文件
camtool -s 3 -o output.h264

# 完整流水线：采集 → 转换 → 编码 → 打包 → 网络发送
camtool -s 15 -a 192.168.1.100 -p 8888

# 自定义分辨率和帧率
camtool -w 1280 -h 720 -f 30 -s 3 -o output.h264

# 调试模式（显示处理进度）
camtool -d -s 3 -o output.h264
```

### 处理阶段 (-s)

`-s` 参数使用位掩码控制处理流程：

| 位 | 功能 | 累加值 |
|----|------|--------|
| bit 0 | 色彩转换 | +1 |
| bit 1 | H.264 编码 | +2 |
| bit 2 | RTP 打包 | +4 |
| bit 3 | 网络发送 | +8 |

常用组合：
- `0` - 仅采集（原始数据）
- `1` - 采集 + 转换
- `3` - 采集 + 转换 + 编码 (默认)
- `7` - 采集 + 转换 + 编码 + 打包
- `15` - 完整流水线（含网络发送）

### 完整选项列表

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `-?` | 显示帮助 | - |
| `-v` | 显示版本 | - |
| `-d` | 调试模式（显示进度） | OFF |
| `-s N` | 处理阶段（位掩码，见上表） | 3 |
| `-i DEV` | 视频设备路径 | /dev/video0 |
| `-o FILE` | 输出文件 | - |
| `-a IP` | 服务器 IP 地址 | - |
| `-p PORT` | 服务器端口 | - |
| `-c N` | 采集像素格式 (0:YUYV, 1:YUV420) | 0 |
| `-w N` | 视频宽度 | 640 |
| `-h N` | 视频高度 | 480 |
| `-r N` | 码率 (kbps) | 1000 |
| `-f N` | 帧率 | 15 |
| `-g N` | GOP 大小 | 12 |

## API 参考

### Capture - 视频采集

```cpp
class Capture {
public:
    explicit Capture(const CaptureParams& params);
    
    void start();                          // 开始采集
    void stop();                           // 停止采集
    Buffer getData();                      // 获取一帧（可能返回空）
    
    // 图像参数控制
    std::optional<ControlRange> queryBrightness() const;
    std::optional<int> getBrightness() const;
    bool setBrightness(int value);
    // 类似接口：Contrast, Saturation
    
    int getImageSize() const;
    const CaptureParams& getParams() const;
};
```

### Convert - 色彩转换

```cpp
class Convert {
public:
    explicit Convert(const ConvertParams& params);
    
    Buffer convert(const Buffer& input);   // 转换图像
    int getOutputSize() const;             // 输出缓冲区大小
    const ConvertParams& getParams() const;
};
```

### Encoder - H.264 编码

```cpp
class Encoder {
public:
    explicit Encoder(const EncoderParams& params);
    
    std::optional<EncodedFrame> getHeaders();  // 获取 SPS/PPS
    EncodedFrame encode(const Buffer& input);  // 编码一帧
    
    // 动态参数调整
    bool setQP(int qp);           // 量化参数 (0-51)
    bool setGOP(int gop);         // GOP 大小
    bool setBitrate(int bitrate); // 码率 (kbps)
    bool setFramerate(int fps);   // 帧率
    void forceIFrame();           // 强制下一帧为 I 帧
    
    const EncoderParams& getParams() const;
};
```

### RTPPacker - RTP 打包

```cpp
class RTPPacker {
public:
    explicit RTPPacker(const RTPPackerParams& params);
    
    void put(const Buffer& input);         // 放入 NAL 单元
    std::optional<Buffer> get();           // 获取 RTP 包
    const RTPPackerParams& getParams() const;
};
```

### Network - 网络传输

```cpp
class Network {
public:
    explicit Network(const NetworkParams& params);
    
    int send(const void* data, int size);  // 发送数据
    int send(const Buffer& buffer);        // 发送缓冲区
    int receive(void* data, int size);     // 接收数据
    bool isConnected() const;              // 检查连接状态
    const NetworkParams& getParams() const;
};
```

### Timestamp - 时间戳绘制

```cpp
class Timestamp {
public:
    explicit Timestamp(const TimestampParams& params);
    
    void draw(uint8_t* image);                      // 绘制时间戳
    void drawText(uint8_t* image, const char* text); // 绘制自定义文字
    const TimestampParams& getParams() const;
};
```

## 异常处理

所有模块在发生错误时抛出相应的异常类：

| 异常类 | 说明 |
|--------|------|
| `CaptureException` | 采集错误（设备打开失败、格式不支持等） |
| `ConvertException` | 转换错误（初始化失败、格式不支持等） |
| `EncodeException` | 编码错误（编解码器初始化失败、编码失败等） |
| `NetworkException` | 网络错误（连接失败、发送失败等） |
| `PackException` | 打包错误（缓冲区溢出等） |

所有异常类都继承自 `camera_toolkitException`（继承自 `std::runtime_error`）。

## 支持的格式

### 像素格式

| 格式 | 说明 |
|------|------|
| `PixelFormat::YUYV` | YUYV 4:2:2 |
| `PixelFormat::YUV420` | YUV 4:2:0 平面格式 |
| `PixelFormat::RGB565` | RGB565 |
| `PixelFormat::RGB24` | RGB24 |

### 网络协议

| 协议 | 说明 |
|------|------|
| `NetworkType::UDP` | UDP 协议（默认） |
| `NetworkType::TCP` | TCP 协议 |

## 许可证

本项目采用 MIT 许可证，详见 [LICENSE](LICENSE) 文件。

## 贡献

欢迎提交 Issue 和 Pull Request！

1. Fork 本仓库
2. 创建功能分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add some amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 提交 Pull Request
