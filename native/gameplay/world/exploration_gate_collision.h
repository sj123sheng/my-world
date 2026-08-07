#pragma once

#include "native/engine/world/environment_collision.h"

#include <cstdint>
#include <vector>

class ExplorationContent;

// 当前探索状态下仍关闭的路径门碰撞集合。状态不在此对象持久化，
// 每次从 ExplorationContent 构建以避免出现第二份门状态真相。
class ExplorationGateCollision {
 public:
  static ExplorationGateCollision fromContent(const ExplorationContent& content);

  BuildingContact resolve(float& x, float& y, float radius,
                          float height) const;
  bool blocks(int32_t gateId) const;
  const std::vector<BuildingBox>& boxes() const { return collision_.boxes(); }

 private:
  explicit ExplorationGateCollision(std::vector<BuildingBox> boxes,
                                    std::vector<int32_t> gateIds);

  BuildingCollision collision_;
  std::vector<int32_t> gateIds_;
};
