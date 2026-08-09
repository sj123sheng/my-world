// test_terrain_biome.cpp: 分区生态调色板纯函数回归。
//
// 编译：c++ -std=c++17 -isystem "$CXX_STDLIB" -I. \
//   tests/test_terrain_biome.cpp -o /tmp/test_terrain_biome

#include "native/engine/render/terrain_biome.h"
#include "native/generated/world_layout.gen.h"

#include <cassert>
#include <cmath>

namespace {

bool colorFinite(const glm::vec3& color) {
  return std::isfinite(color.x) && std::isfinite(color.y) &&
         std::isfinite(color.z);
}

bool colorInUnitRange(const glm::vec3& color) {
  return color.x >= 0.0f && color.x <= 1.0f && color.y >= 0.0f &&
         color.y <= 1.0f && color.z >= 0.0f && color.z <= 1.0f;
}

}  // namespace

int main() {
  // 全部生成分区都有专属调色板（互不回退默认）：保证六区生态差异化成立。
  for (const auto& district : WorldLayout::kDistricts) {
    const TerrainBiomePalette palette = TerrainBiomeFor(district.districtId);
    assert(colorFinite(palette.grass));
    assert(colorFinite(palette.sand));
    assert(colorFinite(palette.rock));
    assert(colorInUnitRange(palette.grass));
    assert(colorInUnitRange(palette.sand));
    assert(colorInUnitRange(palette.rock));
    // 草色是分区主色：必须与默认回退色不同，否则该区没有生态识别度。
    const TerrainBiomePalette fallback = TerrainBiomeFor({});
    const bool differsFromFallback =
        std::abs(palette.grass.x - fallback.grass.x) > 1e-4f ||
        std::abs(palette.grass.y - fallback.grass.y) > 1e-4f ||
        std::abs(palette.grass.z - fallback.grass.z) > 1e-4f;
    assert(differsFromFallback);
  }

  // 灰烬荒原的草色应偏灰（去饱和），与翠风低地的鲜绿区分。
  const TerrainBiomePalette ashen = TerrainBiomeFor("ashen_wastes");
  const TerrainBiomePalette westlands = TerrainBiomeFor("westlands");
  const float ashenSaturation =
      std::max(ashen.grass.x, std::max(ashen.grass.y, ashen.grass.z)) -
      std::min(ashen.grass.x, std::min(ashen.grass.y, ashen.grass.z));
  const float westSaturation =
      std::max(westlands.grass.x,
               std::max(westlands.grass.y, westlands.grass.z)) -
      std::min(westlands.grass.x,
               std::min(westlands.grass.y, westlands.grass.z));
  assert(ashenSaturation < westSaturation);

  // 未知分区回退默认调色板，且确定性（同输入同输出）。
  const TerrainBiomePalette unknownA = TerrainBiomeFor("no_such_district");
  const TerrainBiomePalette unknownB = TerrainBiomeFor("no_such_district");
  assert(unknownA.grass == unknownB.grass);
  assert(unknownA.sand == unknownB.sand);
  assert(unknownA.rock == unknownB.rock);
  assert(unknownA.grass == TerrainBiomeFor({}).grass);

  return 0;
}
