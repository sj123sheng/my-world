#include "native/gameplay/targeting/target_lock_controller.h"

#include "native/gameplay/targeting/soft_targeting.h"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <unordered_map>

namespace {

constexpr float kPi = 3.14159265358979323846f;

// 连招维持距离 = 自动获取距离 × 1.25：连招活跃期间容忍当前目标
// 轻微漂移，超出后即使连招中也重选，避免拉着打不到的目标。
constexpr float kComboHoldDistanceScale = 1.25f;

struct MeasuredCandidate {
  EntityId id = 0;
  float distance = 0.0f;
  float angle = 0.0f;
};

}  // namespace

TargetLockController::TargetLockController(TargetLockConfig config)
    : config_(config) {
  const TargetLockConfig defaults;
  if (!std::isfinite(config_.maxDistance) || config_.maxDistance <= 0.0f) {
    config_.maxDistance = defaults.maxDistance;
  }
  config_.maxAngle = std::isfinite(config_.maxAngle)
                         ? std::clamp(config_.maxAngle, 0.0f, kPi)
                         : defaults.maxAngle;
  if (config_.markerFadeMs < 0) {
    config_.markerFadeMs = defaults.markerFadeMs;
  }
}

TargetLockResult TargetLockController::updateAutomatic(
    Vec2 player, float cameraYaw,
    const std::vector<TargetLockCandidate>& candidates,
    bool attackTriggered, bool comboActive, Tick now) {
  TargetLockResult result;
  result.mode = TargetLockMode::Automatic;

  if (!player.finite() || !std::isfinite(cameraYaw)) {
    currentId_.reset();
    return result;
  }

  // 重复 id 全部拒绝（与 SoftTargeting 同契约）。
  std::unordered_map<EntityId, std::size_t> idCounts;
  for (const TargetLockCandidate& candidate : candidates) {
    if (candidate.id > 0) {
      ++idCounts[candidate.id];
    }
  }

  // 测量与过滤：存活、可攻击、获取距离与角度上限内。当前目标另按
  // 连招维持距离测量（不看角度），供连招保持判定。
  std::vector<MeasuredCandidate> valid;
  valid.reserve(candidates.size());
  std::optional<MeasuredCandidate> current;
  const float holdDistance = config_.maxDistance * kComboHoldDistanceScale;
  for (const TargetLockCandidate& candidate : candidates) {
    if (candidate.id == 0 || idCounts[candidate.id] != 1 || !candidate.alive ||
        !candidate.attackable) {
      continue;
    }
    const TargetCandidate target{static_cast<int32_t>(candidate.id),
                                 candidate.position};
    const TargetMeasure measure = MeasureTarget(player, cameraYaw, target);
    if (!measure.valid) {
      continue;
    }
    if (currentId_.has_value() && candidate.id == *currentId_ &&
        measure.distance <= holdDistance) {
      current = MeasuredCandidate{candidate.id, measure.distance,
                                  measure.angle};
    }
    if (measure.distance <= config_.maxDistance &&
        measure.angle <= config_.maxAngle) {
      valid.push_back(
          MeasuredCandidate{candidate.id, measure.distance, measure.angle});
    }
  }

  std::optional<MeasuredCandidate> selected;
  if (comboActive && current.has_value()) {
    // 连招期间保持当前目标，防止抖动换目标。
    selected = current;
  } else if (!valid.empty()) {
    // 距离优先，同距按镜头前方（角度），再按 id 保证确定性。
    selected = *std::min_element(
        valid.begin(), valid.end(),
        [](const MeasuredCandidate& lhs, const MeasuredCandidate& rhs) {
          return std::tie(lhs.distance, lhs.angle, lhs.id) <
                 std::tie(rhs.distance, rhs.angle, rhs.id);
        });
  }

  if (selected.has_value()) {
    currentId_ = selected->id;
    result.id = selected->id;
    result.distance = selected->distance;
    result.angle = selected->angle;
  } else {
    currentId_.reset();
  }

  if (attackTriggered || comboActive) {
    lastActivityMs_ = now;
  }
  result.showMarker =
      result.id.has_value() &&
      (attackTriggered || comboActive ||
       (lastActivityMs_ != kNoActivity &&
        now - lastActivityMs_ <= config_.markerFadeMs));
  return result;
}

void TargetLockController::invalidate(EntityId id) {
  if (currentId_.has_value() && *currentId_ == id) {
    currentId_.reset();
  }
}

void TargetLockController::clear() {
  mode_ = TargetLockMode::Automatic;
  currentId_.reset();
  lastActivityMs_ = kNoActivity;
}
