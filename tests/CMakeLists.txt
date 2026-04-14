# ==============================================================================
# 单元测试
# ==============================================================================
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)

# 不安装 googletest，仅用于测试
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ==============================================================================
# RTPPacker 测试
# ==============================================================================
add_executable(test_rtp_packer test_rtp_packer.cpp)

target_link_libraries(test_rtp_packer
    PRIVATE
        camera_toolkit
        GTest::gtest_main
)

target_include_directories(test_rtp_packer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_BINARY_DIR}/../include
)

add_test(NAME RTPPackerTests COMMAND test_rtp_packer)

# ==============================================================================
# Timestamp 测试
# ==============================================================================
add_executable(test_timestamp test_timestamp.cpp)

target_link_libraries(test_timestamp
    PRIVATE
        camera_toolkit
        GTest::gtest_main
)

target_include_directories(test_timestamp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_BINARY_DIR}/../include
)

add_test(NAME TimestampTests COMMAND test_timestamp)
