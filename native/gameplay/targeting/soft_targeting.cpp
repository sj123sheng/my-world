#include "native/gameplay/targeting/soft_targeting.h"

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

std::optional<TargetSelection> SoftTargeting::select(
    Vec2 player, float cameraYaw,
    const std::vector<TargetCandidate>& candidates) const {
  if (!player.finite() || !std::isfinite(cameraYaw)) {
    return std::nullopt;
  }

  const Vec2 cameraForward{std::sin(cameraYaw), std::cos(cameraYaw)};
  std::optional<TargetSelection> best;
  std::unordered_map<int32_t, std::size_t> idCounts;

  for (const TargetCandidate& candidate : candidates) {
    if (candidate.id > 0) {
      ++idCounts[candidate.id];
    }
  }

  for (const TargetCandidate& candidate : candidates) {
    if (candidate.id <= 0 || idCounts[candidate.id] != 1 ||
        !candidate.position.finite()) {
      continue;
    }

    const Vec2 offset = candidate.position - player;
    const float distance = std::hypot(offset.x, offset.y);
    if (!std::isfinite(distance) || distance == 0.0f ||
        distance > config_.maxDistance) {
      continue;
    }

    const Vec2 direction{offset.x / distance, offset.y / distance};
    const float cosine = std::clamp(
        direction.x * cameraForward.x + direction.y * cameraForward.y,
        -1.0f, 1.0f);
    const float angle = std::acos(cosine);
    if (!std::isfinite(angle) || angle > config_.maxAngle) {
      continue;
    }

    TargetSelection selection{candidate.id, distance, angle, direction};
    if (!best || std::tie(selection.angle, selection.distance, selection.id) <
                     std::tie(best->angle, best->distance, best->id)) {
      best = selection;
    }
  }

  return best;
}
