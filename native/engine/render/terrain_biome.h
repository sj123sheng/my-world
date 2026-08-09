#pragma once

#include <glm/vec3.hpp>

#include <string_view>

// 分区生态调色（原神式 biome 语言）：每个 district 有自己的沙/草/岩
// 配色，地形片段着色器按世界坐标在相邻分区之间平滑加权混合。
// 纯函数：未知 districtId 回退全局默认配色，确定性可测试。
struct TerrainBiomePalette {
  glm::vec3 sand;
  glm::vec3 grass;
  glm::vec3 rock;
};

// 全局默认配色（与升级前 setTerrainColors 一致）：未配置分区时使用。
constexpr TerrainBiomePalette kDefaultTerrainBiome{
    {0.72f, 0.64f, 0.46f},
    {0.30f, 0.48f, 0.27f},
    {0.44f, 0.44f, 0.48f},
};

// 按 districtId 返回分区调色板；未知 id 回退 kDefaultTerrainBiome。
// 配色设计：启明台地暖亮草色 / 翠风低地鲜绿 / 辉光湖畔青绿 /
// 中枢回廊中性偏干 / 灰烬荒原灰褐 / 圣所高地金石。
inline TerrainBiomePalette TerrainBiomeFor(std::string_view districtId) {
  if (districtId == "spawn_plateau") {
    return {{0.78f, 0.70f, 0.50f}, {0.44f, 0.60f, 0.32f}, {0.48f, 0.47f, 0.50f}};
  }
  if (districtId == "westlands") {
    return {{0.74f, 0.68f, 0.48f}, {0.36f, 0.58f, 0.30f}, {0.46f, 0.46f, 0.48f}};
  }
  if (districtId == "gimmerlake") {
    return {{0.80f, 0.72f, 0.52f}, {0.32f, 0.56f, 0.38f}, {0.44f, 0.47f, 0.50f}};
  }
  if (districtId == "central_corridor") {
    return {{0.72f, 0.64f, 0.48f}, {0.42f, 0.52f, 0.34f}, {0.50f, 0.48f, 0.48f}};
  }
  if (districtId == "ashen_wastes") {
    return {{0.58f, 0.52f, 0.44f}, {0.46f, 0.42f, 0.36f}, {0.40f, 0.38f, 0.40f}};
  }
  if (districtId == "sanctum_highlands") {
    return {{0.76f, 0.66f, 0.46f}, {0.50f, 0.56f, 0.32f}, {0.56f, 0.52f, 0.46f}};
  }
  return kDefaultTerrainBiome;
}
