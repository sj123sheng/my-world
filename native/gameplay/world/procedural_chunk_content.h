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

// foliageSalt 仅用于确定性隔离测试；生产调用保持默认固定盐 0x20。
ProceduralChunkContent GenerateProceduralChunk(
    uint64_t worldSeed, ChunkCoord coord, const TerrainHeightfield& terrain,
    uint64_t foliageSalt = 0x20ULL);
