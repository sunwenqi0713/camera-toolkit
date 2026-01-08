/**
 * @file Timestamp.cpp
 * @brief 时间戳绘制类实现
 */
#include "camera_toolkit/timestamp.h"

#include <array>
#include <cstring>
#include <ctime>
#include <iostream>

namespace camera_toolkit {

namespace {

constexpr int ASCII_MAX = 127; /**< ASCII最大值 */

/**
 * @brief 小字符绘制数据结构体
 */
struct DrawChar {
  unsigned char ascii;     /**< ASCII码 */
  unsigned char pix[8][7]; /**< 像素数据(8行7列) */
};

/**
 * @brief 大字符绘制数据结构体
 */
struct BigChar {
  unsigned char ascii;       /**< ASCII码 */
  unsigned char pix[16][14]; /**< 像素数据(16行14列) */
};

/**
 * @brief 字符绘制表(小字符)
 */
static const DrawChar drawTable[] = {
    {' ',
     {{0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0}}},
    {'0',
     {{0, 0, 1, 1, 1, 0, 0},
      {0, 1, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 2, 2, 1},
      {1, 2, 1, 2, 1, 2, 1},
      {1, 2, 1, 2, 1, 2, 1},
      {1, 2, 2, 1, 1, 2, 1},
      {0, 1, 2, 2, 2, 1, 0},
      {0, 0, 1, 1, 1, 0, 0}}},
    {'1',
     {{0, 0, 0, 1, 0, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 1, 2, 2, 1, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 1, 2, 2, 2, 1, 0},
      {0, 0, 1, 1, 1, 0, 0}}},
    {'2',
     {{0, 0, 1, 1, 1, 0, 0},
      {0, 1, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 2, 1},
      {0, 1, 1, 2, 2, 1, 0},
      {0, 1, 2, 1, 1, 0, 0},
      {1, 2, 1, 1, 1, 1, 0},
      {1, 2, 2, 2, 2, 2, 1},
      {0, 1, 1, 1, 1, 1, 0}}},
    {'3',
     {{0, 0, 1, 1, 1, 0, 0},
      {0, 1, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 2, 1},
      {0, 1, 1, 2, 2, 1, 0},
      {0, 1, 0, 1, 1, 2, 1},
      {1, 2, 1, 1, 1, 2, 1},
      {0, 1, 2, 2, 2, 1, 0},
      {0, 0, 1, 1, 1, 0, 0}}},
    {'4',
     {{0, 0, 0, 0, 1, 0, 0},
      {0, 0, 0, 1, 2, 1, 0},
      {0, 0, 1, 2, 2, 1, 0},
      {0, 1, 2, 1, 2, 1, 0},
      {1, 2, 2, 2, 2, 2, 1},
      {0, 1, 1, 1, 2, 1, 0},
      {0, 0, 0, 1, 2, 1, 0},
      {0, 0, 0, 0, 1, 0, 0}}},
    {'5',
     {{0, 1, 1, 1, 1, 1, 0},
      {1, 2, 2, 2, 2, 2, 1},
      {1, 2, 1, 1, 1, 1, 0},
      {1, 2, 2, 2, 2, 1, 0},
      {0, 1, 1, 1, 1, 2, 0},
      {0, 1, 1, 1, 1, 2, 0},
      {1, 2, 2, 2, 2, 1, 0},
      {0, 1, 1, 1, 1, 0, 0}}},
    {'6',
     {{0, 0, 1, 1, 1, 1, 0},
      {0, 1, 2, 2, 2, 2, 1},
      {1, 2, 1, 1, 1, 1, 0},
      {1, 2, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 1, 1, 1, 2, 1},
      {0, 1, 2, 2, 2, 1, 0},
      {0, 0, 1, 1, 1, 0, 0}}},
    {'7',
     {{0, 1, 1, 1, 1, 1, 0},
      {1, 2, 2, 2, 2, 2, 1},
      {0, 1, 1, 1, 1, 2, 1},
      {0, 0, 0, 1, 2, 1, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 1, 2, 1, 0, 0, 0},
      {0, 1, 2, 1, 0, 0, 0},
      {0, 0, 1, 0, 0, 0, 0}}},
    {'8',
     {{0, 0, 1, 1, 1, 0, 0},
      {0, 1, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 2, 1},
      {0, 1, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 1, 1, 1, 2, 1},
      {0, 1, 2, 2, 2, 1, 0},
      {0, 0, 1, 1, 1, 0, 0}}},
    {'9',
     {{0, 0, 1, 1, 1, 0, 0},
      {0, 1, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 2, 1},
      {0, 1, 2, 2, 2, 2, 1},
      {0, 1, 1, 1, 1, 2, 1},
      {1, 2, 1, 1, 1, 2, 1},
      {0, 1, 2, 2, 2, 1, 0},
      {0, 0, 1, 1, 1, 0, 0}}},
    {':',
     {{0, 0, 1, 1, 0, 0, 0},
      {0, 1, 2, 2, 1, 0, 0},
      {0, 1, 2, 2, 1, 0, 0},
      {0, 0, 1, 1, 0, 0, 0},
      {0, 0, 1, 1, 0, 0, 0},
      {0, 1, 2, 2, 1, 0, 0},
      {0, 1, 2, 2, 1, 0, 0},
      {0, 0, 1, 1, 0, 0, 0}}},
    {'-',
     {{0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0},
      {0, 0, 1, 1, 1, 0, 0},
      {0, 1, 2, 2, 2, 1, 0},
      {0, 0, 1, 1, 1, 0, 0},
      {0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0}}},
    {'(',
     {{0, 0, 0, 1, 0, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 1, 2, 1, 0, 0, 0},
      {0, 1, 2, 1, 0, 0, 0},
      {0, 1, 2, 1, 0, 0, 0},
      {0, 1, 2, 1, 0, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 0, 1, 0, 0, 0}}},
    {')',
     {{0, 0, 0, 1, 0, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 0, 1, 2, 1, 0},
      {0, 0, 0, 1, 2, 1, 0},
      {0, 0, 0, 1, 2, 1, 0},
      {0, 0, 0, 1, 2, 1, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 0, 1, 0, 0, 0}}},
    {'A',
     {{0, 0, 1, 1, 1, 0, 0},
      {0, 1, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 2, 2, 2, 2, 1},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {0, 1, 0, 0, 0, 1, 0}}},
    {'B',
     {{0, 1, 1, 1, 1, 0, 0},
      {1, 2, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 2, 2, 2, 1, 0},
      {0, 1, 1, 1, 1, 0, 0}}},
    {'C',
     {{0, 0, 1, 1, 1, 0, 0},
      {0, 1, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 1, 0, 0, 1, 0},
      {1, 2, 1, 0, 0, 1, 0},
      {1, 2, 1, 1, 1, 2, 1},
      {0, 1, 2, 2, 2, 1, 0},
      {0, 0, 1, 1, 1, 0, 0}}},
    {'D',
     {{0, 1, 1, 1, 1, 0, 0},
      {1, 2, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 2, 2, 2, 1, 0},
      {0, 1, 1, 1, 1, 0, 0}}},
    {'E',
     {{0, 1, 1, 1, 1, 1, 0},
      {1, 2, 2, 2, 2, 2, 1},
      {1, 2, 1, 1, 1, 1, 0},
      {1, 2, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 0, 0},
      {1, 2, 1, 1, 1, 1, 0},
      {1, 2, 2, 2, 2, 2, 1},
      {0, 1, 1, 1, 1, 1, 0}}},
    {'F',
     {{0, 1, 1, 1, 1, 1, 0},
      {1, 2, 2, 2, 2, 2, 1},
      {1, 2, 1, 1, 1, 1, 0},
      {1, 2, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 0, 0},
      {1, 2, 1, 0, 0, 0, 0},
      {1, 2, 1, 0, 0, 0, 0},
      {0, 1, 0, 0, 0, 0, 0}}},
    {'G',
     {{0, 0, 1, 1, 1, 0, 0},
      {0, 1, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 1, 1, 1, 1, 0},
      {1, 2, 1, 2, 2, 2, 1},
      {1, 2, 1, 1, 1, 2, 1},
      {0, 1, 2, 2, 2, 1, 0},
      {0, 0, 1, 1, 1, 0, 0}}},
    {'H',
     {{0, 1, 0, 0, 0, 1, 0},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 2, 2, 2, 2, 1},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {0, 1, 0, 0, 0, 1, 0}}},
    {'I',
     {{0, 0, 1, 1, 1, 0, 0},
      {0, 1, 2, 2, 2, 1, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 1, 2, 2, 2, 1, 0},
      {0, 0, 1, 1, 1, 0, 0}}},
    {'J',
     {{0, 0, 1, 1, 1, 1, 0},
      {0, 1, 2, 2, 2, 2, 1},
      {0, 0, 1, 1, 1, 2, 1},
      {0, 0, 0, 0, 1, 2, 1},
      {0, 1, 0, 0, 1, 2, 1},
      {1, 2, 1, 1, 1, 2, 1},
      {0, 1, 2, 2, 2, 1, 0},
      {0, 0, 1, 1, 1, 0, 0}}},
    {'K',
     {{0, 1, 0, 0, 0, 1, 0},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 1, 2, 1, 0},
      {1, 2, 1, 2, 1, 0, 0},
      {1, 2, 2, 2, 1, 0, 0},
      {1, 2, 1, 1, 2, 1, 0},
      {1, 2, 1, 0, 1, 2, 1},
      {0, 1, 0, 0, 0, 1, 0}}},
    {'L',
     {{0, 1, 0, 0, 0, 0, 0},
      {1, 2, 1, 0, 0, 0, 0},
      {1, 2, 1, 0, 0, 0, 0},
      {1, 2, 1, 0, 0, 0, 0},
      {1, 2, 1, 0, 0, 0, 0},
      {1, 2, 1, 1, 1, 0, 0},
      {1, 2, 2, 2, 2, 1, 0},
      {0, 1, 1, 1, 1, 0, 0}}},
    {'M',
     {{0, 1, 1, 0, 1, 1, 0},
      {1, 2, 2, 1, 2, 2, 1},
      {1, 2, 1, 2, 1, 2, 1},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {0, 1, 0, 0, 0, 1, 0}}},
    {'N',
     {{0, 1, 0, 0, 0, 1, 0},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 2, 1, 1, 2, 1},
      {1, 2, 1, 2, 1, 2, 1},
      {1, 2, 1, 1, 2, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {0, 1, 0, 0, 0, 1, 0}}},
    {'O',
     {{0, 0, 1, 1, 1, 0, 0},
      {0, 1, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 1, 1, 2, 1},
      {0, 1, 2, 2, 2, 1, 0},
      {0, 0, 1, 1, 1, 0, 0}}},
    {'P',
     {{0, 1, 1, 1, 1, 0, 0},
      {1, 2, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 0, 0},
      {1, 2, 1, 0, 0, 0, 0},
      {1, 2, 1, 0, 0, 0, 0},
      {0, 1, 0, 0, 0, 0, 0}}},
    {'R',
     {{0, 1, 1, 1, 1, 0, 0},
      {1, 2, 2, 2, 2, 1, 0},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 2, 2, 2, 1, 0},
      {1, 2, 1, 2, 1, 0, 0},
      {1, 2, 1, 1, 2, 1, 0},
      {1, 2, 1, 0, 1, 2, 1},
      {0, 1, 0, 0, 0, 1, 0}}},
    {'S',
     {{0, 0, 1, 1, 1, 1, 0},
      {0, 1, 2, 2, 2, 2, 1},
      {1, 2, 1, 1, 1, 1, 0},
      {0, 1, 2, 2, 2, 1, 0},
      {0, 0, 1, 1, 1, 2, 1},
      {0, 1, 1, 1, 1, 2, 1},
      {1, 2, 2, 2, 2, 1, 0},
      {0, 1, 1, 1, 1, 0, 0}}},
    {'T',
     {{0, 1, 1, 1, 1, 1, 0},
      {1, 2, 2, 2, 2, 2, 1},
      {0, 1, 1, 2, 1, 1, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 0, 1, 0, 0, 0}}},
    {'U',
     {{0, 1, 0, 0, 0, 1, 0},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 1, 1, 2, 1},
      {0, 1, 2, 2, 2, 2, 1},
      {0, 0, 1, 1, 1, 1, 0}}},
    {'V',
     {{0, 1, 0, 0, 0, 1, 0},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {0, 1, 2, 1, 2, 1, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 0, 1, 0, 0, 0}}},
    {'W',
     {{0, 1, 0, 0, 0, 1, 0},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 0, 1, 2, 1},
      {1, 2, 1, 1, 1, 2, 1},
      {1, 2, 1, 2, 1, 2, 1},
      {1, 2, 1, 2, 1, 2, 1},
      {0, 1, 2, 1, 2, 1, 0},
      {0, 0, 1, 0, 1, 0, 0}}},
    {'X',
     {{0, 1, 0, 0, 0, 1, 0},
      {1, 2, 1, 0, 1, 2, 1},
      {0, 1, 2, 1, 2, 1, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 1, 2, 1, 2, 1, 0},
      {1, 2, 1, 0, 1, 2, 1},
      {0, 1, 0, 0, 0, 1, 0}}},
    {'Y',
     {{0, 1, 0, 0, 0, 1, 0},
      {1, 2, 1, 0, 1, 2, 1},
      {0, 1, 2, 1, 2, 1, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 0, 0, 1, 0, 0, 0}}},
    {'Z',
     {{0, 1, 1, 1, 1, 1, 0},
      {1, 2, 2, 2, 2, 2, 1},
      {0, 1, 1, 1, 2, 1, 0},
      {0, 0, 1, 2, 1, 0, 0},
      {0, 1, 2, 1, 0, 0, 0},
      {1, 2, 1, 1, 1, 1, 0},
      {1, 2, 2, 2, 2, 2, 1},
      {0, 1, 1, 1, 1, 1, 0}}},
};

constexpr size_t DRAW_TABLE_SIZE = sizeof(drawTable) / sizeof(DrawChar); /**< 绘制表大小 */

// 字符查找表
static std::array<const unsigned char*, ASCII_MAX> smallCharArrPtr; /**< 小字符指针表 */
static std::array<const unsigned char*, ASCII_MAX> bigCharArrPtr;   /**< 大字符指针表 */
static std::array<BigChar, DRAW_TABLE_SIZE> bigTable;               /**< 大字符表 */
static bool initialized = false;                                    /**< 初始化标志 */

/**
 * @brief 初始化字符绘制表
 */
void initializeChars() {
  if (initialized) return;

  // 构建大字符表
  for (size_t i = 0; i < DRAW_TABLE_SIZE; i++) {
    bigTable[i].ascii = drawTable[i].ascii;
    for (int x = 0; x < 14; x++) {
      for (int y = 0; y < 16; y++) {
        bigTable[i].pix[y][x] = drawTable[i].pix[y / 2][x / 2];
      }
    }
  }

  // 初始化所有字符指针为空格
  for (int i = 0; i < ASCII_MAX; i++) {
    smallCharArrPtr[i] = &drawTable[0].pix[0][0];
    bigCharArrPtr[i] = &bigTable[0].pix[0][0];
  }

  // 构建查找表
  for (size_t i = 0; i < DRAW_TABLE_SIZE; i++) {
    int ascii = static_cast<int>(drawTable[i].ascii);
    smallCharArrPtr[ascii] = &drawTable[i].pix[0][0];
    bigCharArrPtr[ascii] = &bigTable[i].pix[0][0];
  }

  initialized = true;
}

/**
 * @brief 绘制指定长度的文本
 * @param image 图像数据指针
 * @param startX 起始X坐标
 * @param startY 起始Y坐标
 * @param width 图像宽度
 * @param text 文本内容
 * @param len 文本长度
 * @param factor 放大因子(0=小, 1=大)
 * @return 成功返回0
 */
int drawTextN(unsigned char* image, unsigned int startX, unsigned int startY, unsigned int width, const char* text,
              int len, unsigned int factor) {
  if (startX > width / 2) {
    startX -= len * (6 * (factor + 1));
  }

  if (startX + len * 6 * (factor + 1) >= width) {
    len = (width - startX - 1) / (6 * (factor + 1));
  }

  int lineOffset = width - 7 * (factor + 1);
  int nextCharOffs = width * 8 * (factor + 1) - 6 * (factor + 1);

  unsigned char* imagePtr = image + startX + startY * width;
  const auto& charArrPtr = factor ? bigCharArrPtr : smallCharArrPtr;

  for (int pos = 0; pos < len; pos++) {
    int posCheck = static_cast<int>(static_cast<unsigned char>(text[pos]));
    if (posCheck >= ASCII_MAX) posCheck = ' ';

    const unsigned char* charPtr = charArrPtr[posCheck];

    for (unsigned int y = 8 * (factor + 1); y > 0; y--) {
      for (unsigned int x = 7 * (factor + 1); x > 0; x--) {
        if (posCheck >= 0) {
          switch (*charPtr) {
            case 1:
              *imagePtr = 0;
              break;
            case 2:
              *imagePtr = 255;
              break;
            default:
              break;
          }
        }
        imagePtr++;
        charPtr++;
      }
      imagePtr += lineOffset;
    }
    imagePtr -= nextCharOffs;
  }

  return 0;
}

/**
 * @brief 绘制完整文本(支持换行)
 * @param image 图像数据指针
 * @param startX 起始X坐标
 * @param startY 起始Y坐标
 * @param width 图像宽度
 * @param text 文本内容(使用\\n作为换行符)
 * @param factor 放大因子(0=小, 1=大)
 * @return 成功返回0
 */
int drawTextFull(unsigned char* image, unsigned int startX, unsigned int startY, unsigned int width, const char* text,
                 unsigned int factor) {
  constexpr const char* NEWLINE = "\\n";
  int numNl = 0;
  const char* end = text;
  const int lineSpace = (factor + 1) * 9;

  // 计算换行数
  while ((end = strstr(end, NEWLINE))) {
    numNl++;
    end += 2;
  }

  startY -= lineSpace * numNl;

  const char* begin = text;
  end = text;

  while ((end = strstr(end, NEWLINE))) {
    int len = end - begin;
    drawTextN(image, startX, startY, width, begin, len, factor);
    end += 2;
    begin = end;
    startY += lineSpace;
  }

  drawTextN(image, startX, startY, width, begin, strlen(begin), factor);
  return 0;
}

}  // anonymous namespace

/**
 * @brief Timestamp类的PIMPL实现
 */
class Timestamp::Impl {
 public:
  /**
   * @brief 构造函数
   * @param params 时间戳参数
   */
  explicit Impl(const TimestampParams& params) : params_(params) {
    initializeChars();
    std::cout << "+++ Timestamp opened" << std::endl;
  }

  /**
   * @brief 析构函数
   */
  ~Impl() { std::cout << "+++ Timestamp closed" << std::endl; }

  /**
   * @brief 在图像上绘制时间戳
   * @param image 图像数据指针(YUV的Y平面)
   */
  void draw(uint8_t* image) {
    time_t captureTime = time(nullptr);
    char timestamp[64];
    struct tm* tmTimestamp = localtime(&captureTime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S (%Z)", tmTimestamp);
    drawTextFull(image, params_.startX, params_.startY, params_.videoWidth, timestamp, params_.factor);
  }

  /**
   * @brief 在图像上绘制自定义文字
   * @param image 图像数据指针
   * @param text 要绘制的文字
   */
  void drawText(uint8_t* image, const char* text) {
    drawTextFull(image, params_.startX, params_.startY, params_.videoWidth, text, params_.factor);
  }

  /**
   * @brief 获取时间戳参数
   * @return 时间戳参数引用
   */
  const TimestampParams& getParams() const { return params_; }

 private:
  TimestampParams params_; /**< 时间戳参数 */
};

// ============================================================================
// 公共接口实现
// ============================================================================

Timestamp::Timestamp(const TimestampParams& params) : pImpl_(std::make_unique<Impl>(params)) {}

Timestamp::~Timestamp() = default;

void Timestamp::draw(uint8_t* image) { pImpl_->draw(image); }

void Timestamp::drawText(uint8_t* image, const char* text) { pImpl_->drawText(image, text); }

const TimestampParams& Timestamp::getParams() const { return pImpl_->getParams(); }

}  // namespace camera_toolkit
