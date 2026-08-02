#include "native/engine/render/digit_atlas.h"

#include <cassert>
#include <cmath>
#include <string>

int main() {
  const DigitAtlas atlas = DigitAtlas::build();

  // 图集尺寸：10 个单元水平排列。
  assert(atlas.cellWidth == 16);
  assert(atlas.cellHeight == 20);
  assert(atlas.width == 160);
  assert(atlas.height == 20);
  assert(atlas.pixels.size() ==
         static_cast<std::size_t>(atlas.width) *
             static_cast<std::size_t>(atlas.height) * 4u);

  // 每个数字都有点亮像素，且点亮数不超过字形区域。
  const int maxLit = DigitAtlas::kGlyphColumns * DigitAtlas::kScale *
                     DigitAtlas::kGlyphRows * DigitAtlas::kScale;
  int previousLit[10];
  for (char digit = '0'; digit <= '9'; ++digit) {
    const int lit = atlas.litPixels(digit);
    assert(lit > 0);
    assert(lit <= maxLit);
    previousLit[digit - '0'] = lit;
  }

  // 不同数字的像素签名不同（防止字形复制粘贴错误）。
  auto signature = [&atlas](char digit) {
    std::string bits;
    const int index = digit - '0';
    for (int y = 0; y < atlas.height; ++y) {
      for (int x = index * atlas.cellWidth; x < (index + 1) * atlas.cellWidth;
           ++x) {
        const std::size_t offset =
            (static_cast<std::size_t>(y) *
                 static_cast<std::size_t>(atlas.width) +
             static_cast<std::size_t>(x)) * 4u;
        bits.push_back(atlas.pixels[offset + 3] > 0u ? '1' : '0');
      }
    }
    return bits;
  };
  for (char a = '0'; a <= '9'; ++a) {
    for (char b = a + 1; b <= '9'; ++b) {
      assert(signature(a) != signature(b));
    }
  }

  // 点亮像素均为不透明白色。
  for (std::size_t index = 0; index < atlas.pixels.size(); index += 4) {
    if (atlas.pixels[index + 3] > 0u) {
      assert(atlas.pixels[index] == 255u);
      assert(atlas.pixels[index + 1] == 255u);
      assert(atlas.pixels[index + 2] == 255u);
      assert(atlas.pixels[index + 3] == 255u);
    }
  }

  // UV 矩形：按数字顺序排列、不重叠、范围合法。
  float lastU1 = 0.0f;
  for (char digit = '0'; digit <= '9'; ++digit) {
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
    assert(atlas.uvRect(digit, u0, v0, u1, v1));
    assert(u0 >= lastU1);
    assert(u1 > u0);
    assert(u1 <= 1.0f);
    assert(v0 == 0.0f && v1 == 1.0f);
    assert(std::abs((u1 - u0) * atlas.width - atlas.cellWidth) < 0.001f);
    lastU1 = u1;
  }

  // 非数字被拒绝。
  float u0, v0, u1, v1;
  assert(!atlas.uvRect('a', u0, v0, u1, v1));
  assert(!atlas.uvRect('/', u0, v0, u1, v1));
  assert(!DigitAtlas::isDigit('x'));
  assert(atlas.litPixels('x') == 0);
  return 0;
}
