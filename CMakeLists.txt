# ==============================================================================
# Camera toolKit
# ==============================================================================
cmake_minimum_required(VERSION 3.16)

# 版本信息
set(CK_VERSION_MAJOR 2)
set(CK_VERSION_MINOR 0)
set(CK_VERSION_PATCH 0)
set(CK_VERSION "${CK_VERSION_MAJOR}.${CK_VERSION_MINOR}.${CK_VERSION_PATCH}")

project(camera_toolkit 
    VERSION ${CK_VERSION}
    DESCRIPTION "Camera toolKit"
    LANGUAGES CXX
)

# ==============================================================================
# CMake选项
# ==============================================================================
option(DEBUG "Enable debug mode" OFF)
option(BUILD_SHARED_LIBS "Build shared library" ON)
option(BUILD_TOOL "Build camera_toolkit command-line tool" ON)
option(BUILD_EXAMPLES "Build example programs" OFF)

# ==============================================================================
# 编译器设置
# ==============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 导出编译命令(用于IDE和工具)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Debug/Release配置
if(DEBUG)
    set(CMAKE_BUILD_TYPE Debug)
    add_compile_definitions(DEBUG_MODE)
    message(STATUS "Build type: Debug")
else()
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "Build type: Release")
endif()

# 编译选项
add_compile_options(-Wall -Wextra -Wpedantic)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
else()
    add_compile_options(-O2)
endif()

# ==============================================================================
# 查找依赖
# ==============================================================================
find_package(PkgConfig REQUIRED)

# FFmpeg组件
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWSCALE REQUIRED libswscale)

message(STATUS "Found FFmpeg components:")
message(STATUS "  libavcodec: ${AVCODEC_VERSION}")
message(STATUS "  libavutil: ${AVUTIL_VERSION}")
message(STATUS "  libswscale: ${SWSCALE_VERSION}")

# ==============================================================================
# 配置头文件
# ==============================================================================
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/camera_toolkit/config.h
    @ONLY
)

# ==============================================================================
# 库源文件
# ==============================================================================
set(camera_toolkit_SOURCES
    src/capture.cpp
    src/convert.cpp
    src/encoder.cpp
    src/network.cpp
    src/rtp_packer.cpp
    src/timestamp.cpp
)

set(camera_toolkit_HEADERS
    include/camera_toolkit.h
    include/camera_toolkit/common.h
    include/camera_toolkit/capture.h
    include/camera_toolkit/convert.h
    include/camera_toolkit/encoder.h
    include/camera_toolkit/network.h
    include/camera_toolkit/rtp_packer.h
    include/camera_toolkit/timestamp.h
)

# ==============================================================================
# camera_toolkit库目标
# ==============================================================================
add_library(camera_toolkit ${camera_toolkit_SOURCES})
add_library(camera_toolkit::camera_toolkit ALIAS camera_toolkit)

target_include_directories(camera_toolkit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${AVCODEC_INCLUDE_DIRS}
        ${AVUTIL_INCLUDE_DIRS}
        ${SWSCALE_INCLUDE_DIRS}
)

target_link_libraries(camera_toolkit
    PRIVATE
        ${AVCODEC_LIBRARIES}
        ${AVUTIL_LIBRARIES}
        ${SWSCALE_LIBRARIES}
)

target_link_directories(camera_toolkit
    PRIVATE
        ${AVCODEC_LIBRARY_DIRS}
        ${AVUTIL_LIBRARY_DIRS}
        ${SWSCALE_LIBRARY_DIRS}
)

# 设置库版本
set_target_properties(camera_toolkit PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${CK_VERSION_MAJOR}
    PUBLIC_HEADER "${camera_toolkit_HEADERS}"
)

# ==============================================================================
# camera_toolkit命令行工具
# ==============================================================================
if(BUILD_TOOL)
    add_executable(camtool src/camera_toolkit.cpp)

    target_link_libraries(camtool
        PRIVATE
            camera_toolkit
    )

    target_include_directories(camtool
        PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}/include
    )
endif()

# ==============================================================================
# 安装配置
# ==============================================================================
include(GNUInstallDirs)

# 安装库
install(TARGETS camera_toolkit
    EXPORT camera_toolkit-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/camera_toolkit
)

# 安装主头文件
install(FILES include/camera_toolkit.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# 安装生成的配置头文件
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/camera_toolkit/config.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/camera_toolkit
)

# 安装camtool命令行工具
if(BUILD_TOOL)
    install(TARGETS camtool
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# ==============================================================================
# CMake包配置 (用于find_package)
# ==============================================================================
install(EXPORT camera_toolkit-targets
    FILE camera_toolkit-targets.cmake
    NAMESPACE camera_toolkit::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/camera_toolkit
)

include(CMakePackageConfigHelpers)

# 生成版本文件
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/camera_toolkit-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# 配置文件
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/camera_toolkit-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/camera_toolkit-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/camera_toolkit
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/camera_toolkit-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/camera_toolkit-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/camera_toolkit
)

# ==============================================================================
# pkg-config文件
# ==============================================================================
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/camera_toolkit.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/camera_toolkit.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/camera_toolkit.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# ==============================================================================
# 构建信息摘要
# ==============================================================================
message(STATUS "")
message(STATUS "=== camera_toolkit Configuration Summary ===")
message(STATUS "Version:         ${CK_VERSION}")
message(STATUS "Platform:        ${PLAT}")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "Shared libs:     ${BUILD_SHARED_LIBS}")
message(STATUS "Build tool:      ${BUILD_TOOL}")
message(STATUS "Install prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "====================================")
message(STATUS "")
