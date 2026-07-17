#include "combat_region.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

float finiteFloat(double value) {
  const double maximum = static_cast<double>(std::numeric_limits<float>::max());
  if (!std::isfinite(value)) return 0.0f;
  return static_cast<float>(std::max(-maximum, std::min(maximum, value)));
}

}  // namespace

CombatRegion::CombatRegion(CombatRegionConfig config) : config_(config) {
  if (!config_.center.finite()) config_.center = {};
  if (!std::isfinite(config_.radius) || config_.radius <= 0.0f) {
    config_.radius = 0.0f;
  }
}

bool CombatRegion::contains(Vec2 point, float tolerance) const noexcept {
  if (!point.finite() || !std::isfinite(tolerance) || tolerance < 0.0f) return false;

  const double deltaX = static_cast<double>(point.x) - config_.center.x;
  const double deltaY = static_cast<double>(point.y) - config_.center.y;
  const double allowedRadius = static_cast<double>(config_.radius) + tolerance;
  return std::hypot(deltaX, deltaY) <= allowedRadius;
}

Vec2 CombatRegion::projectInside(Vec2 point, float inset) const noexcept {
  if (!point.finite()) return config_.center;
  if (!std::isfinite(inset) || inset < 0.0f) inset = 0.0f;

  const double allowedRadius =
      std::max(0.0, static_cast<double>(config_.radius) - inset);
  const double deltaX = static_cast<double>(point.x) - config_.center.x;
  const double deltaY = static_cast<double>(point.y) - config_.center.y;
  const double distance = std::hypot(deltaX, deltaY);
  if (distance <= allowedRadius) return point;
  if (!(distance > 0.0) || !std::isfinite(distance)) return config_.center;

  const double scale = allowedRadius / distance;
  const Vec2 projected = {
      finiteFloat(static_cast<double>(config_.center.x) + deltaX * scale),
      finiteFloat(static_cast<double>(config_.center.y) + deltaY * scale),
  };
  return projected.finite() ? projected : config_.center;
}

Vec2 stableSeparation(EntityId selfId, Vec2 selfPosition, EntityId otherId,
                      Vec2 otherPosition, float minimumDistance) noexcept {
  if (selfId == 0 || otherId == 0 || selfId == otherId || !selfPosition.finite() ||
      !otherPosition.finite() || !std::isfinite(minimumDistance) ||
      minimumDistance <= 0.0f) {
    return {};
  }

  const double deltaX = static_cast<double>(selfPosition.x) - otherPosition.x;
  const double deltaY = static_cast<double>(selfPosition.y) - otherPosition.y;
  const double distance = std::hypot(deltaX, deltaY);
  if (!std::isfinite(distance) || distance >= minimumDistance) return {};

  const double correction = (static_cast<double>(minimumDistance) - distance) * 0.5;
  if (distance == 0.0) {
    return {selfId < otherId ? -finiteFloat(correction) : finiteFloat(correction),
            0.0f};
  }

  const Vec2 separation = {
      finiteFloat(deltaX / distance * correction),
      finiteFloat(deltaY / distance * correction),
  };
  return separation.finite() ? separation : Vec2{};
}
