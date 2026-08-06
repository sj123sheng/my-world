#include "native/engine/world/world_grid.h"

#include <algorithm>
#include <cmath>

namespace {

int32_t clampInt(int32_t value, int32_t lo, int32_t hi) {
  return value < lo ? lo : (value > hi ? hi : value);
}

}  // namespace

WorldGrid::WorldGrid(WorldGridConfig config) : config_(config) {
  if (config_.countX < 1) config_.countX = 1;
  if (config_.countY < 1) config_.countY = 1;
  if (config_.streamingRadius < 0) config_.streamingRadius = 0;
}

int32_t WorldGrid::chunkIndexAt(Vec2 position) const {
  float px = position.x;
  float py = position.y;
  if (!std::isfinite(px)) px = 0.0f;
  if (!std::isfinite(py)) py = 0.0f;
  px = std::clamp(px, 0.0f, 1.0f);
  py = std::clamp(py, 0.0f, 1.0f);
  int32_t cx = static_cast<int32_t>(px * static_cast<float>(config_.countX));
  int32_t cy = static_cast<int32_t>(py * static_cast<float>(config_.countY));
  cx = clampInt(cx, 0, config_.countX - 1);
  cy = clampInt(cy, 0, config_.countY - 1);
  return cy * config_.countX + cx;
}

bool WorldGrid::setStreamingRadius(int32_t radius) {
  const int32_t clamped = std::max(radius, 0);
  if (clamped == config_.streamingRadius) return false;
  config_.streamingRadius = clamped;
  return true;
}

bool WorldGrid::updateStreaming(Vec2 playerPosition) {
  const int32_t current = chunkIndexAt(playerPosition);
  const int32_t cx = current % config_.countX;
  const int32_t cy = current / config_.countX;
  const int32_t r = config_.streamingRadius;

  // 期望激活集合：玩家所在分块向外扩展 radius 的矩形窗口，
  // 按行主序 id 天然升序生成。
  std::vector<int32_t> desired;
  desired.reserve((2 * r + 1) * (2 * r + 1));
  for (int32_t y = cy - r; y <= cy + r; ++y) {
    for (int32_t x = cx - r; x <= cx + r; ++x) {
      const int32_t clampedX = clampInt(x, 0, config_.countX - 1);
      const int32_t clampedY = clampInt(y, 0, config_.countY - 1);
      desired.push_back(clampedY * config_.countX + clampedX);
    }
  }
  std::sort(desired.begin(), desired.end());
  desired.erase(std::unique(desired.begin(), desired.end()), desired.end());

  if (desired == active_) {
    pendingLoads_.clear();
    pendingUnloads_.clear();
    return false;
  }

  // 差集计算：两个输入均升序，std::set_difference 输出亦升序，
  // 保证加/卸载请求序列确定性。
  pendingLoads_.clear();
  pendingUnloads_.clear();
  std::set_difference(desired.begin(), desired.end(), active_.begin(),
                      active_.end(), std::back_inserter(pendingLoads_));
  std::set_difference(active_.begin(), active_.end(), desired.begin(),
                      desired.end(), std::back_inserter(pendingUnloads_));
  active_ = std::move(desired);
  return true;
}
