#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "native/engine/math/vec2.h"

struct TargetCandidate {
  int32_t id = 0;
  Vec2 position;

  bool operator==(const TargetCandidate& other) const {
    return id == other.id && position == other.position;
  }
};

struct TargetSelection {
  int32_t id = 0;
  float distance = 0.0f;
  float angle = 0.0f;
  Vec2 direction;
};

// 单个候选的纯测量结果：距离/角度/方向。不做范围与角度过滤，
// 过滤阈值由调用方决定（SoftTargeting 与 TargetLockController 共用）。
struct TargetMeasure {
  int32_t id = 0;
  float distance = 0.0f;
  float angle = 0.0f;
  Vec2 direction;
  bool valid = false;
};

// 纯候选测量：相对玩家位置与相机前向测量单个候选。玩家/相机非有限、
// id 非正、位置非有限或与玩家重合时 valid=false。无状态、无过滤。
TargetMeasure MeasureTarget(Vec2 player, float cameraYaw,
                            const TargetCandidate& candidate);

struct SoftTargetingConfig {
  float maxDistance = 0.75f;
  float maxAngle = 1.0471976f;
};

class SoftTargeting {
 public:
  explicit SoftTargeting(SoftTargetingConfig config = {});

  std::optional<TargetSelection> select(
      Vec2 player, float cameraYaw,
      const std::vector<TargetCandidate>& candidates,
      std::optional<int32_t> preferredId = std::nullopt) const;

 private:
  SoftTargetingConfig config_;
};
