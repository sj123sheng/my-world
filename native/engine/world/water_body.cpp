#include "native/engine/world/water_body.h"

#include <algorithm>
#include <cmath>

bool WaterBody::contains(glm::vec2 point) const {
  if (!(halfExtents.x > 0.0f) || !(halfExtents.y > 0.0f)) return false;
  const float dx = (point.x - center.x) / halfExtents.x;
  const float dy = (point.y - center.y) / halfExtents.y;
  return dx * dx + dy * dy <= 1.0f;
}

float WaterBody::shoreFactor(glm::vec2 point) const {
  if (!contains(point) || !(shoreWidth > 0.0f)) return 0.0f;
  const float dx = (point.x - center.x) / halfExtents.x;
  const float dy = (point.y - center.y) / halfExtents.y;
  const float radial = std::sqrt(dx * dx + dy * dy);
  const float localRadius = std::min(halfExtents.x, halfExtents.y);
  const float distanceToEdge = (1.0f - radial) * localRadius;
  return 1.0f - std::clamp(distanceToEdge / shoreWidth, 0.0f, 1.0f);
}
