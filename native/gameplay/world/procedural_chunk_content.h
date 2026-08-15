#pragma once

#include "native/engine/world/terrain_heightfield.h"
#include "native/gameplay/ai/enemy_ai_types.h"

#include <cstdint>
#include <vector>

// 外围分块的纯描述数据；运行时实体由后续接线层按 stableId 管理。
struct ProceduralFoliageSpawn {
  LocalPosition position{};
  uint8_t kind = 0;
  float scale = 1.0f;
};

struct ProceduralEnemySpawn {
  uint64_t stableId = 0;
  LocalPosition position{};
  EnemyArchetype archetype = EnemyArchetype::RiftClaw;
};

struct ProceduralCollectibleSpawn {
  uint64_t stableId = 0;
  LocalPosition position{};
  int32_t itemId = 0;
};

struct ProceduralChunkContent {
  std::vector<ProceduralFoliageSpawn> foliage;
  std::vector<ProceduralEnemySpawn> enemies;
  std::vector<ProceduralCollectibleSpawn> collectibles;
};

ProceduralChunkContent GenerateProceduralChunk(
    uint64_t worldSeed, ChunkCoord coord, const TerrainHeightfield& terrain);

namespace testing {

// 只允许测试改变植被盐，敌人和采集物始终使用生产固定盐。
struct ProceduralGenerationSalts {
  uint64_t foliageSalt = 0x20ULL;
};

ProceduralChunkContent GenerateProceduralChunkForTesting(
    uint64_t worldSeed, ChunkCoord coord, const TerrainHeightfield& terrain,
    ProceduralGenerationSalts salts);

}  // namespace testing
