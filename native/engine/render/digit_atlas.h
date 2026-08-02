#pragma once

#include <cstdint>
#include <vector>

// 程序化数字字形图集：无需字体资源，用内置 5x7 点阵光栅化 0-9，
// 供伤害飘字在 GLES 渲染层以纹理四边形绘制。纯 CPU 构建，可单测。
struct DigitAtlas {
  // RGBA 像素（row-major），每个字形占一个 cellWidth x cellHeight 单元，
  // 按 '0'-'9' 顺序水平排列，共 10 格。
  std::vector<uint8_t> pixels;
  int width = 0;
  int height = 0;
  int cellWidth = 0;
  int cellHeight = 0;

  static constexpr int kGlyphColumns = 5;
  static constexpr int kGlyphRows = 7;
  static constexpr int kScale = 2;  // 每点放大为 2x2 像素，提升可读性

  // 构建图集：10 个字形单元，单元尺寸 (5*2+6) x (7*2+6) = 16x20。
  static DigitAtlas build();

  static bool isDigit(char c) { return c >= '0' && c <= '9'; }

  // 返回字符在图集中的 UV 矩形（OpenGL 约定 v0 为底边）。
  // 非数字返回 false。
  bool uvRect(char c, float& u0, float& v0, float& u1, float& v1) const;

  // 字形 d 在单元内是否点亮了像素（测试辅助）。
  int litPixels(char c) const;
};
