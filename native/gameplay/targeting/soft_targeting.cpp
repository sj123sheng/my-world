#include "native/gameplay/targeting/soft_targeting.h"

#include <algorithm>
#include <cmath>
#include <tuple>

std::optional<TargetSelection> SoftTargeting::select(
    Vec2 player, float cameraYaw,
    const std::vector<TargetCandidate>& candidates) const {
  if (!player.finite() || !std::isfinite(cameraYaw)) {
    return std::nullopt;
  }

  const Vec2 cameraForward{std::sin(cameraYaw), std::cos(cameraYaw)};
  std::optional<TargetSelection> best;

  for (const TargetCandidate& candidate : candidates) {
    if (candidate.id <= 0 || !candidate.position.finite()) {
      continue;
    }

    const Vec2 offset = candidate.position - player;
    const float distance = offset.length();
    if (!std::isfinite(distance) || distance == 0.0f ||
        distance > config_.maxDistance) {
      continue;
    }

    const Vec2 direction = offset * (1.0f / distance);
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
