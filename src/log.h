/**
 * @file log.h
 * @brief Camera Toolkit 内部日志工具
 *
 * 提供统一的日志输出接口，便于后续集中替换为正式日志框架。
 * 仅供库内部源文件使用，不对外暴露。
 */
#pragma once

#include <iostream>
#include <string>

namespace camera_toolkit {
namespace log {

/**
 * @brief 输出信息日志（组件生命周期事件）
 * @param msg 日志消息
 */
inline void info(const std::string& msg) { std::cout << "+++ " << msg << std::endl; }

/**
 * @brief 输出警告日志（非致命异常路径）
 * @param msg 日志消息
 */
inline void warn(const std::string& msg) { std::cerr << "!!! " << msg << std::endl; }

/**
 * @brief 输出错误日志（操作失败）
 * @param msg 日志消息
 */
inline void error(const std::string& msg) { std::cerr << "--- " << msg << std::endl; }

}  // namespace log
}  // namespace camera_toolkit
