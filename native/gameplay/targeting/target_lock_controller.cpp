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

// 手动维持距离 = 自动获取距离 × 1.5：手动锁定容忍更远的目标，
// 避免玩家主动选中的目标因轻微超距立即丢失。
constexpr float kManualHoldDistanceScale = 1.5f;

struct MeasuredCandidate {
  EntityId id = 0;
  float distance = 0.0f;
  float angle = 0.0f;
};

// 收集手动模式有效候选：存活、可攻击、重复 id 拒绝、测量有效且
// 距离不超过给定上限；按 (distance, id) 稳定排序，输入顺序无关。
std::vector<MeasuredCandidate> CollectManualCandidates(
    Vec2 player, float cameraYaw,
    const std::vector<TargetLockCandidate>& candidates, float maxDistance) {
  std::unordered_map<EntityId, std::size_t> idCounts;
  for (const TargetLockCandidate& candidate : candidates) {
    if (candidate.id > 0) {
      ++idCounts[candidate.id];
    }
  }

  std::vector<MeasuredCandidate> measured;
  measured.reserve(candidates.size());
  for (const TargetLockCandidate& candidate : candidates) {
    if (candidate.id == 0 || idCounts[candidate.id] != 1 || !candidate.alive ||
        !candidate.attackable) {
      continue;
    }
    const TargetCandidate target{static_cast<int32_t>(candidate.id),
                                 candidate.position};
    const TargetMeasure measure = MeasureTarget(player, cameraYaw, target);
    if (!measure.valid || measure.distance > maxDistance) {
      continue;
    }
    measured.push_back(
        MeasuredCandidate{candidate.id, measure.distance, measure.angle});
  }

  std::sort(measured.begin(), measured.end(),
            [](const MeasuredCandidate& lhs, const MeasuredCandidate& rhs) {
              return std::tie(lhs.distance, lhs.id) <
                     std::tie(rhs.distance, rhs.id);
            });
  return measured;
}

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

TargetLockResult TargetLockController::cycleManual(
    Vec2 player, float cameraYaw,
    const std::vector<TargetLockCandidate>& candidates, Tick now) {
  (void)now;
  TargetLockResult result;
  if (!player.finite() || !std::isfinite(cameraYaw)) {
    clear();
    return result;
  }

  const std::vector<MeasuredCandidate> sorted = CollectManualCandidates(
      player, cameraYaw, candidates,
      config_.maxDistance * kManualHoldDistanceScale);
  if (sorted.empty()) {
    clear();
    return result;
  }

  // 已处于手动且当前目标仍在候选中：选其后一项并环绕；
  // 否则（首次单击或当前目标已失效）锁定最近目标。
  std::size_t index = 0;
  if (mode_ == TargetLockMode::Manual && currentId_.has_value()) {
    const auto current = std::find_if(
        sorted.begin(), sorted.end(), [this](const MeasuredCandidate& entry) {
          return entry.id == *currentId_;
        });
    if (current != sorted.end()) {
      index = static_cast<std::size_t>(current - sorted.begin() + 1) %
              sorted.size();
    }
  }

  const MeasuredCandidate& selected = sorted[index];
  mode_ = TargetLockMode::Manual;
  currentId_ = selected.id;
  result.id = selected.id;
  result.mode = TargetLockMode::Manual;
  result.distance = selected.distance;
  result.angle = selected.angle;
  // 手动模式锁定环始终显示。
  result.showMarker = true;
  return result;
}

TargetLockResult TargetLockController::releaseManual(
    Vec2 player, float cameraYaw,
    const std::vector<TargetLockCandidate>& candidates, Tick now) {
  mode_ = TargetLockMode::Automatic;
  currentId_.reset();
  // 不触碰 lastActivityMs_：解除锁定不伪造攻击活跃窗口。
  return updateAutomatic(player, cameraYaw, candidates,
                         /*attackTriggered=*/false, /*comboActive=*/false, now);
}

TargetLockResult TargetLockController::refresh(
    Vec2 player, float cameraYaw,
    const std::vector<TargetLockCandidate>& candidates, Tick now) {
  if (mode_ == TargetLockMode::Automatic) {
    return updateAutomatic(player, cameraYaw, candidates,
                           /*attackTriggered=*/false, /*comboActive=*/false,
                           now);
  }

  TargetLockResult result;
  result.mode = TargetLockMode::Manual;
  if (!player.finite() || !std::isfinite(cameraYaw)) {
    clear();
    result.mode = mode_;
    return result;
  }

  const std::vector<MeasuredCandidate> sorted = CollectManualCandidates(
      player, cameraYaw, candidates,
      config_.maxDistance * kManualHoldDistanceScale);

  // 当前目标仍存活且在手动维持距离内：保持。
  if (currentId_.has_value()) {
    const auto current = std::find_if(
        sorted.begin(), sorted.end(), [this](const MeasuredCandidate& entry) {
          return entry.id == *currentId_;
        });
    if (current != sorted.end()) {
      result.id = current->id;
      result.distance = current->distance;
      result.angle = current->angle;
      result.showMarker = true;
      return result;
    }
  }

  // 死亡/超距/卸载：重选最近有效候选。
  if (!sorted.empty()) {
    const MeasuredCandidate& selected = sorted.front();
    currentId_ = selected.id;
    result.id = selected.id;
    result.distance = selected.distance;
    result.angle = selected.angle;
    result.showMarker = true;
    return result;
  }

  // 无候选：回到 Automatic。
  clear();
  result.mode = mode_;
  return result;
}

void TargetLockController::clear() {
  mode_ = TargetLockMode::Automatic;
  currentId_.reset();
  lastActivityMs_ = kNoActivity;
}

float TargetLockController::markerVisibility(Tick now) const {
  if (!currentId_.has_value()) return 0.0f;
  // 手动模式锁定环常亮。
  if (mode_ == TargetLockMode::Manual) return 0.92f;
  if (lastActivityMs_ == kNoActivity) return 0.0f;
  const Tick elapsed = now - lastActivityMs_;
  if (elapsed <= 0) return 0.72f;
  if (config_.markerFadeMs <= 0 || elapsed >= config_.markerFadeMs) return 0.0f;
  const float remaining = 1.0f - static_cast<float>(elapsed) /
                                     static_cast<float>(config_.markerFadeMs);
  return 0.72f * remaining;
}
