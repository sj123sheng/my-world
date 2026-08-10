#include "native/gameplay/world/terrain_wall_collision.h"

#include <algorithm>
#include <cmath>

#include "native/engine/world/terrain_heightfield.h"

namespace {

// 高度场水平梯度（最陡上升方向）：中心差分，边界钳制采样，
// 与 TerrainHeightfield::slopeAt 同口径同采样步长。
Vec2 heightGradient(const TerrainHeightfield& terrain, float x, float y) {
  const float step = terrain.config().slopeSampleStep;
  const float x0 = std::max(x - step, 0.0f);
  const float x1 = std::min(x + step, 1.0f);
  const float y0 = std::max(y - step, 0.0f);
  const float y1 = std::min(y + step, 1.0f);
  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float gx = dx > 0.0f
                       ? (terrain.heightAt(x1, y) - terrain.heightAt(x0, y)) / dx
                       : 0.0f;
  const float gy = dy > 0.0f
                       ? (terrain.heightAt(x, y1) - terrain.heightAt(x, y0)) / dy
                       : 0.0f;
  return {gx, gy};
}

Vec2 normalized(Vec2 v, Vec2 fallback) {
  const float len = v.length();
  if (!std::isfinite(len) || len < 1e-6f) return fallback;
  return v * (1.0f / len);
}

}  // namespace

TerrainWallContact terrainWallContact(const TerrainHeightfield& terrain,
                                      float x, float y, float height,
                                      Vec2 moveDir, float probeDistance) {
  TerrainWallContact contact;
  const Vec2 dir = normalized(moveDir, {0.0f, 0.0f});
  if (dir.length() < 0.5f || !std::isfinite(probeDistance) ||
      probeDistance <= 0.0f || !std::isfinite(height)) {
    return contact;
  }
  const float probeX = std::clamp(x + dir.x * probeDistance, 0.0f, 1.0f);
  const float probeY = std::clamp(y + dir.y * probeDistance, 0.0f, 1.0f);
  contact.groundAhead = terrain.heightAt(probeX, probeY);
  // 可行走抬升上限 = 可行走最大坡度 × 探测距离：缓坡永远不阻挡，
  // 陡坡（墙体）必然阻挡，与 climbSlopeThreshold 单点收敛。
  const float maxStepUp =
      terrain.config().climbSlopeThreshold * probeDistance;
  contact.blocked = contact.groundAhead - height > maxStepUp;
  if (!contact.blocked) return contact;
  contact.climbable = terrain.climbableAt(probeX, probeY);
  // 墙面法线取探测点梯度；梯度退化时回退移动方向，保证滑动投影有定义。
  contact.normal = normalized(heightGradient(terrain, probeX, probeY), dir);
  return contact;
}

Vec2 slideAlongTerrainWall(Vec2 moveDelta, Vec2 wallNormal) {
  const Vec2 normal = normalized(wallNormal, {0.0f, 0.0f});
  if (normal.length() < 0.5f || !moveDelta.finite()) return moveDelta;
  const float intoWall =
      moveDelta.x * normal.x + moveDelta.y * normal.y;
  if (intoWall <= 0.0f) return moveDelta;
  return {moveDelta.x - intoWall * normal.x,
          moveDelta.y - intoWall * normal.y};
}

Vec2 depenetrateTerrainWall(const TerrainHeightfield& terrain, float x,
                            float y, float height, float probeDistance) {
  Vec2 position{x, y};
  if (!position.finite() || !std::isfinite(height) ||
      !std::isfinite(probeDistance) || probeDistance <= 0.0f) {
    return position;
  }
  const float maxStepUp =
      terrain.config().climbSlopeThreshold * probeDistance;
  const float pushStep = probeDistance * 0.5f;
  // 迭代推出：每步沿下坡方向挪动半个探测距离，最多 8 步
  //（总行程 4×probeDistance，覆盖击退/传送级别的嵌入深度）。
  for (int i = 0; i < 8; ++i) {
    const float groundHere = terrain.heightAt(position.x, position.y);
    if (groundHere - height <= maxStepUp) break;
    const Vec2 downhill =
        normalized(heightGradient(terrain, position.x, position.y) * -1.0f,
                   {0.0f, 0.0f});
    if (downhill.length() < 0.5f) break;  // 梯度退化：无确定推出方向。
    position.x = std::clamp(position.x + downhill.x * pushStep, 0.0f, 1.0f);
    position.y = std::clamp(position.y + downhill.y * pushStep, 0.0f, 1.0f);
  }
  return position;
}
