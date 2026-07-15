#include "native/engine/render/camera.h"

#include <cassert>
#include <cmath>
#include <limits>

int main() {
  ThirdPersonCamera camera;
  camera.update({0.5f, 0.5f}, {1.0f, 100.0f}, 0.016f);
  assert(camera.yaw() == 1.0f);
  assert(camera.pitch() == camera.config().maxPitch);

  camera.setDistance(100.0f);
  assert(camera.distance() == camera.config().maxDistance);

  camera.update({1.0f, 1.0f}, {}, 0.1f);
  assert(camera.target().x > 0.5f && camera.target().x < 1.0f);

  camera.update({0.5f, 0.5f},
                {std::numeric_limits<float>::infinity(), 0.0f}, 0.016f);
  assert(std::isfinite(camera.yaw()));
  assert(camera.pitch() == camera.config().defaultPitch);

  ThirdPersonCameraConfig projectionConfig;
  projectionConfig.defaultYaw = std::acos(-1.0f) * 0.5f;
  projectionConfig.defaultPitch = 0.0f;
  projectionConfig.defaultDistance = 0.35f;
  ThirdPersonCamera projectionCamera(projectionConfig);
  const Vec2 position = projectionCamera.position();
  assert(std::abs(position.x + projectionConfig.defaultDistance) < 0.0001f);
  assert(std::abs(position.y) < 0.0001f);
}
