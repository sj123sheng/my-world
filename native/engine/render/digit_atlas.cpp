#include "native/engine/render/digit_atlas.h"

namespace {

// 经典 5x7 点阵数字字形，每行 5 位（最高位 = 最左列）。
constexpr uint8_t kDigitFont[10][7] = {
    {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110},  // 0
    {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},  // 1
    {0b01110, 0b10001, 0b00001, 0b00110, 0b01000, 0b10000, 0b11111},  // 2
    {0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110},  // 3
    {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010},  // 4
    {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110},  // 5
    {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110},  // 6
    {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000},  // 7
    {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110},  // 8
    {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100},  // 9
};

}  // namespace

DigitAtlas DigitAtlas::build() {
  DigitAtlas atlas;
  atlas.cellWidth = kGlyphColumns * kScale + 6;
  atlas.cellHeight = kGlyphRows * kScale + 6;
  atlas.width = atlas.cellWidth * 10;
  atlas.height = atlas.cellHeight;
  atlas.pixels.assign(static_cast<std::size_t>(atlas.width) *
                          static_cast<std::size_t>(atlas.height) * 4u,
                      0u);

  for (int digit = 0; digit < 10; ++digit) {
    const int cellOriginX = digit * atlas.cellWidth + 3;
    const int cellOriginY = 3;
    for (int fontRow = 0; fontRow < kGlyphRows; ++fontRow) {
      // 纹理行 0 对应字形底部（OpenGL v=0 在底部），保证采样方向正确。
      const int bottomUpRow = kGlyphRows - 1 - fontRow;
      const uint8_t bits = kDigitFont[digit][fontRow];
      for (int col = 0; col < kGlyphColumns; ++col) {
        if ((bits & (1 << (kGlyphColumns - 1 - col))) == 0) {
          continue;
        }
        for (int sy = 0; sy < kScale; ++sy) {
          for (int sx = 0; sx < kScale; ++sx) {
            const int px = cellOriginX + col * kScale + sx;
            const int py = cellOriginY + bottomUpRow * kScale + sy;
            const std::size_t index =
                (static_cast<std::size_t>(py) *
                     static_cast<std::size_t>(atlas.width) +
                 static_cast<std::size_t>(px)) * 4u;
            atlas.pixels[index + 0] = 255u;
            atlas.pixels[index + 1] = 255u;
            atlas.pixels[index + 2] = 255u;
            atlas.pixels[index + 3] = 255u;
          }
        }
      }
    }
  }
  return atlas;
}

bool DigitAtlas::uvRect(char c, float& u0, float& v0, float& u1,
                        float& v1) const {
  if (!isDigit(c) || width <= 0 || height <= 0) {
    return false;
  }
  const int digit = c - '0';
  u0 = static_cast<float>(digit * cellWidth) / static_cast<float>(width);
  u1 = static_cast<float>((digit + 1) * cellWidth) / static_cast<float>(width);
  v0 = 0.0f;
  v1 = 1.0f;
  return true;
}

int DigitAtlas::litPixels(char c) const {
  if (!isDigit(c) || width <= 0 || height <= 0) {
    return 0;
  }
  const int digit = c - '0';
  int count = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = digit * cellWidth; x < (digit + 1) * cellWidth; ++x) {
      const std::size_t index = (static_cast<std::size_t>(y) *
                                     static_cast<std::size_t>(width) +
                                 static_cast<std::size_t>(x)) * 4u;
      if (pixels[index + 3] > 0u) {
        ++count;
      }
    }
  }
  return count;
}
