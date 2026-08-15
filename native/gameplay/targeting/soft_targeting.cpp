#include "native/gameplay/targeting/soft_targeting.h"

#include "native/engine/math/camera_ground_basis.h"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <unordered_map>

namespace {

constexpr float kPi = 3.14159265358979323846f;

}  // namespace

SoftTargeting::SoftTargeting(SoftTargetingConfig config) : config_(config) {
  const SoftTargetingConfig defaults;
  if (!std::isfinite(config_.maxDistance) || config_.maxDistance <= 0.0f) {
    config_.maxDistance = defaults.maxDistance;
  }
  config_.maxAngle = std::isfinite(config_.maxAngle)
                         ? std::clamp(config_.maxAngle, 0.0f, kPi)
                         : defaults.maxAngle;
}

TargetMeasure MeasureTarget(Vec2 player, float cameraYaw,
                            const TargetCandidate& candidate) {
  TargetMeasure measure;
  measure.id = candidate.id;
  if (!player.finite() || !std::isfinite(cameraYaw) || candidate.id <= 0 ||
      !candidate.position.finite()) {
    return measure;
  }

  const Vec2 cameraForward = CameraGroundBasisForYaw(cameraYaw).forward;
  const Vec2 offset = candidate.position - player;
  const float distance = std::hypot(offset.x, offset.y);
  if (!std::isfinite(distance) || distance == 0.0f) {
    return measure;
  }

  const Vec2 direction{offset.x / distance, offset.y / distance};
  const float cosine = std::clamp(
      direction.x * cameraForward.x + direction.y * cameraForward.y,
      -1.0f, 1.0f);
  const float angle = std::acos(cosine);
  if (!std::isfinite(angle)) {
    return measure;
  }

  measure.distance = distance;
  measure.angle = angle;
  measure.direction = direction;
  measure.valid = true;
  return measure;
}

std::optional<TargetSelection> SoftTargeting::select(
    Vec2 player, float cameraYaw,
    const std::vector<TargetCandidate>& candidates,
    std::optional<int32_t> preferredId) const {
  if (!player.finite() || !std::isfinite(cameraYaw)) {
    return std::nullopt;
  }

  std::optional<TargetSelection> best;
  std::optional<TargetSelection> preferred;
  std::unordered_map<int32_t, std::size_t> idCounts;

  for (const TargetCandidate& candidate : candidates) {
    if (candidate.id > 0) {
      ++idCounts[candidate.id];
    }
  }

  for (const TargetCandidate& candidate : candidates) {
    if (candidate.id <= 0 || idCounts[candidate.id] != 1) {
      continue;
    }
    const TargetMeasure measure = MeasureTarget(player, cameraYaw, candidate);
    if (!measure.valid || measure.distance > config_.maxDistance ||
        measure.angle > config_.maxAngle) {
      continue;
    }

    TargetSelection selection{measure.id, measure.distance, measure.angle,
                              measure.direction};
    if (preferredId.has_value() && selection.id == *preferredId) {
      preferred = selection;
    }
    if (!best || std::tie(selection.angle, selection.distance, selection.id) <
                     std::tie(best->angle, best->distance, best->id)) {
      best = selection;
    }
  }

  return preferred.has_value() ? preferred : best;
}
