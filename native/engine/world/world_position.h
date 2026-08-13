#pragma once

#include "engine/math/vec2.h"

#include <cstddef>
#include <cstdint>

struct ChunkCoord {
  int64_t x = 0;
  int64_t y = 0;

  bool operator==(const ChunkCoord& other) const noexcept {
    return x == other.x && y == other.y;
  }

  bool operator!=(const ChunkCoord& other) const noexcept {
    return !(*this == other);
  }

  bool operator<(const ChunkCoord& other) const noexcept {
    return x != other.x ? x < other.x : y < other.y;
  }
};

struct LocalPosition {
  float x = 0.0f;
  float y = 0.0f;
};

struct WorldPosition {
  ChunkCoord chunk{};
  LocalPosition local{};
};

WorldPosition NormalizeWorldPosition(ChunkCoord chunk, double localX,
                                     double localY) noexcept;
Vec2 RelativeWorldPosition(const WorldPosition& target,
                           const WorldPosition& origin) noexcept;
uint64_t StableChunkHash(uint64_t worldSeed, ChunkCoord chunk,
                         uint64_t salt) noexcept;

struct ChunkCoordHash {
  size_t operator()(ChunkCoord coord) const noexcept;
};
