#include "native/gameplay/world/procedural_chunk_content.h"

#include "native/engine/world/world_position.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

constexpr uint64_t kTerrainSalt = 0x10ULL;
constexpr uint64_t kEnemySalt = 0x30ULL;
constexpr uint64_t kCollectibleSalt = 0x40ULL;
constexpr int kMaxAttempts = 32;
constexpr size_t kMaxFoliage = 24;
constexpr size_t kMaxEnemies = 3;
constexpr size_t kMaxCollectibles = 4;
constexpr float kMinLocal = 0.08f;
constexpr float kLocalSpan = 0.84f;
constexpr float kMaxSlope = 0.55f;

uint64_t Mix64(uint64_t value) noexcept {
  value += 0x9e3779b97f4a7c15ULL;
  value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31U);
}

uint64_t AttemptHash(uint64_t worldSeed, ChunkCoord coord, uint64_t contentSalt,
                     uint32_t attempt) noexcept {
  // 地形盐先构成与位置相关的候选状态，再由内容盐分流；任何一类的盐变化
  // 都不会影响另外两类候选。
  const uint64_t terrainState =
      StableChunkHash(worldSeed, coord, kTerrainSalt ^ static_cast<uint64_t>(attempt));
  return StableChunkHash(terrainState, coord, contentSalt);
}

float UnitFloat(uint64_t value) noexcept {
  constexpr uint64_t kMask = (1ULL << 24U) - 1ULL;
  return static_cast<float>((value >> 40U) & kMask) /
         static_cast<float>(kMask);
}

LocalPosition CandidatePosition(uint64_t hash) noexcept {
  return {kMinLocal + kLocalSpan * UnitFloat(hash),
          kMinLocal + kLocalSpan * UnitFloat(Mix64(hash))};
}

bool IsValidPosition(const TerrainHeightfield& terrain, ChunkCoord coord,
                     LocalPosition position) noexcept {
  const float height = terrain.heightAt(coord, position.x, position.y);
  const float slope = terrain.slopeAt(coord, position.x, position.y);
  return std::isfinite(height) && std::isfinite(slope) && slope < kMaxSlope &&
         height >= terrain.config().waterLevel;
}

uint64_t StableSpawnId(uint64_t worldSeed, ChunkCoord coord,
                       uint64_t contentSalt, uint32_t slot) noexcept {
  // 类别与槽位写入低 16 位，令当前块的敌人与采集物即便哈希高位相同也
  // 保持可重复的无冲突 ID；高位仍由世界种子和块坐标决定。
  const uint64_t hash = StableChunkHash(worldSeed, coord, contentSalt);
  return (hash & ~0xffffULL) |
         ((contentSalt & 0xffULL) << 8U) |
         (static_cast<uint64_t>(slot) & 0xffULL);
}

EnemyArchetype OrdinaryArchetype(uint64_t hash) noexcept {
  constexpr EnemyArchetype kOrdinary[] = {
      EnemyArchetype::RiftClaw, EnemyArchetype::Priest, EnemyArchetype::Guard,
      EnemyArchetype::Bruiser, EnemyArchetype::Caster, EnemyArchetype::Elite,
  };
  return kOrdinary[hash % (sizeof(kOrdinary) / sizeof(kOrdinary[0]))];
}

bool IsCoreChunk(ChunkCoord coord) noexcept {
  return coord == ChunkCoord{0, 0};
}

}  // namespace

ProceduralChunkContent GenerateProceduralChunk(uint64_t worldSeed, ChunkCoord coord,
                                                const TerrainHeightfield& terrain,
                                                uint64_t foliageSalt) {
  ProceduralChunkContent content;
  if (IsCoreChunk(coord)) return content;

  content.foliage.reserve(kMaxFoliage);
  content.enemies.reserve(kMaxEnemies);
  content.collectibles.reserve(kMaxCollectibles);

  for (uint32_t attempt = 0; attempt < kMaxAttempts; ++attempt) {
    const uint64_t hash = AttemptHash(worldSeed, coord, foliageSalt, attempt);
    const LocalPosition position = CandidatePosition(hash);
    if (content.foliage.size() < kMaxFoliage &&
        IsValidPosition(terrain, coord, position)) {
      content.foliage.push_back(
          {position, static_cast<uint8_t>(Mix64(hash) % 4U),
           0.80f + 0.40f * UnitFloat(Mix64(Mix64(hash)))});
    }
  }
  for (uint32_t attempt = 0; attempt < kMaxAttempts; ++attempt) {
    const uint64_t hash = AttemptHash(worldSeed, coord, kEnemySalt, attempt);
    const LocalPosition position = CandidatePosition(hash);
    if (content.enemies.size() < kMaxEnemies &&
        IsValidPosition(terrain, coord, position)) {
      content.enemies.push_back(
          {StableSpawnId(worldSeed, coord, kEnemySalt, attempt), position,
           OrdinaryArchetype(Mix64(hash))});
    }
  }
  for (uint32_t attempt = 0; attempt < kMaxAttempts; ++attempt) {
    const uint64_t hash = AttemptHash(worldSeed, coord, kCollectibleSalt, attempt);
    const LocalPosition position = CandidatePosition(hash);
    if (content.collectibles.size() < kMaxCollectibles &&
        IsValidPosition(terrain, coord, position)) {
      content.collectibles.push_back(
          {StableSpawnId(worldSeed, coord, kCollectibleSalt, attempt), position,
           1001 + static_cast<int32_t>(Mix64(hash) % 4U)});
    }
  }

  std::sort(content.enemies.begin(), content.enemies.end(),
            [](const ProceduralEnemySpawn& lhs, const ProceduralEnemySpawn& rhs) {
              return lhs.stableId < rhs.stableId;
            });
  std::sort(content.collectibles.begin(), content.collectibles.end(),
            [](const ProceduralCollectibleSpawn& lhs,
               const ProceduralCollectibleSpawn& rhs) {
              return lhs.stableId < rhs.stableId;
            });
  return content;
}
