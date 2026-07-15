#include "native/engine/render/camera.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <utility>

namespace {

bool close(float actual, float expected) {
  return std::abs(actual - expected) < 0.000001f;
}

void assertReset(const ThirdPersonCamera& camera) {
  assert(std::isfinite(camera.yaw()));
  assert(std::isfinite(camera.pitch()));
  assert(std::isfinite(camera.distance()));
  assert(camera.target().finite());
  assert(camera.yaw() == camera.config().defaultYaw);
  assert(camera.pitch() == camera.config().defaultPitch);
  assert(camera.distance() == camera.config().defaultDistance);
  assert(camera.target().x == 0.5f);
  assert(camera.target().y == 0.5f);
}

void assertValidConfig(const ThirdPersonCameraConfig& config) {
  assert(std::isfinite(config.defaultYaw));
  assert(std::isfinite(config.defaultPitch));
  assert(std::isfinite(config.minPitch));
  assert(std::isfinite(config.maxPitch));
  assert(std::isfinite(config.defaultDistance));
  assert(std::isfinite(config.minDistance));
  assert(std::isfinite(config.maxDistance));
  assert(std::isfinite(config.followSharpness));
  assert(config.minPitch <= config.maxPitch);
  assert(config.defaultPitch >= config.minPitch);
  assert(config.defaultPitch <= config.maxPitch);
  assert(config.minDistance <= config.maxDistance);
  assert(config.minDistance > 0.0f);
  assert(config.maxDistance > 0.0f);
  assert(config.defaultDistance >= config.minDistance);
  assert(config.defaultDistance <= config.maxDistance);
}

void moveAwayFromDefaults(ThirdPersonCamera& camera) {
  camera.update({1.0f, -1.0f}, {0.25f, 0.1f}, 0.1f);
  camera.setDistance(camera.config().maxDistance);
}

}  // namespace

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
  assertReset(camera);

  moveAwayFromDefaults(camera);
  camera.update({std::numeric_limits<float>::quiet_NaN(), 0.5f}, {}, 0.016f);
  assertReset(camera);
  moveAwayFromDefaults(camera);
  camera.update({0.5f, 0.5f}, {},
                std::numeric_limits<float>::infinity());
  assertReset(camera);
  moveAwayFromDefaults(camera);
  camera.setDistance(std::numeric_limits<float>::quiet_NaN());
  assertReset(camera);

  ThirdPersonCamera smoothCamera;
  const Vec2 smoothTarget{1.0f, -0.5f};
  constexpr float fixedDt = 0.1f;
  smoothCamera.update(smoothTarget, {}, fixedDt);
  const float expectedFollow =
      1.0f - std::exp(-smoothCamera.config().followSharpness * fixedDt);
  assert(close(smoothCamera.target().x,
               0.5f + (smoothTarget.x - 0.5f) * expectedFollow));
  assert(close(smoothCamera.target().y,
               0.5f + (smoothTarget.y - 0.5f) * expectedFollow));

  ThirdPersonCamera repeatedCamera;
  repeatedCamera.update(smoothTarget, {}, fixedDt);
  assert(repeatedCamera.target().x == smoothCamera.target().x);
  assert(repeatedCamera.target().y == smoothCamera.target().y);

  ThirdPersonCameraConfig projectionConfig;
  projectionConfig.defaultYaw = std::acos(-1.0f) * 0.5f;
  projectionConfig.defaultPitch = 0.0f;
  projectionConfig.defaultDistance = 0.35f;
  ThirdPersonCamera projectionCamera(projectionConfig);
  const Vec2 position = projectionCamera.position();
  assert(std::abs(position.x - (0.5f - projectionConfig.defaultDistance)) <
         0.0001f);
  assert(std::abs(position.y - 0.5f) < 0.0001f);

  ThirdPersonCameraConfig nonFiniteConfig;
  nonFiniteConfig.defaultYaw = std::numeric_limits<float>::quiet_NaN();
  nonFiniteConfig.defaultPitch = std::numeric_limits<float>::infinity();
  nonFiniteConfig.minPitch = -std::numeric_limits<float>::infinity();
  nonFiniteConfig.maxPitch = std::numeric_limits<float>::quiet_NaN();
  nonFiniteConfig.defaultDistance =
      std::numeric_limits<float>::quiet_NaN();
  nonFiniteConfig.minDistance = -std::numeric_limits<float>::infinity();
  nonFiniteConfig.maxDistance = std::numeric_limits<float>::infinity();
  nonFiniteConfig.followSharpness =
      std::numeric_limits<float>::quiet_NaN();
  ThirdPersonCamera nonFiniteCamera(nonFiniteConfig);
  assertValidConfig(nonFiniteCamera.config());
  const ThirdPersonCameraConfig builtInDefaults;
  assert(nonFiniteCamera.config().defaultYaw == builtInDefaults.defaultYaw);
  assert(nonFiniteCamera.config().defaultPitch ==
         builtInDefaults.defaultPitch);
  assert(nonFiniteCamera.config().minPitch == builtInDefaults.minPitch);
  assert(nonFiniteCamera.config().maxPitch == builtInDefaults.maxPitch);
  assert(nonFiniteCamera.config().defaultDistance ==
         builtInDefaults.defaultDistance);
  assert(nonFiniteCamera.config().minDistance ==
         builtInDefaults.minDistance);
  assert(nonFiniteCamera.config().maxDistance ==
         builtInDefaults.maxDistance);
  assert(nonFiniteCamera.config().followSharpness ==
         builtInDefaults.followSharpness);
  assertReset(nonFiniteCamera);

  ThirdPersonCameraConfig invertedConfig;
  invertedConfig.minPitch = 0.8f;
  invertedConfig.maxPitch = -0.8f;
  invertedConfig.defaultPitch = 20.0f;
  invertedConfig.minDistance = 2.0f;
  invertedConfig.maxDistance = 1.0f;
  invertedConfig.defaultDistance = -20.0f;
  invertedConfig.followSharpness = -1.0f;
  ThirdPersonCamera invertedCamera(invertedConfig);
  assertValidConfig(invertedCamera.config());
  assert(invertedCamera.config().minPitch == builtInDefaults.minPitch);
  assert(invertedCamera.config().maxPitch == builtInDefaults.maxPitch);
  assert(invertedCamera.config().minDistance == builtInDefaults.minDistance);
  assert(invertedCamera.config().maxDistance == builtInDefaults.maxDistance);
  assert(invertedCamera.config().followSharpness ==
         builtInDefaults.followSharpness);
  assertReset(invertedCamera);
  invertedCamera.update({1.0f, 1.0f}, {0.0f, 100.0f}, fixedDt);
  assert(std::isfinite(invertedCamera.pitch()));
  invertedCamera.setDistance(100.0f);
  assert(std::isfinite(invertedCamera.distance()));

  for (const auto invalidDistances : {
           std::pair<float, float>{0.0f, 0.6f},
           std::pair<float, float>{0.2f, 0.0f},
           std::pair<float, float>{-0.2f, 0.6f},
           std::pair<float, float>{0.2f, -0.6f},
       }) {
    ThirdPersonCameraConfig invalidConfig;
    invalidConfig.minDistance = invalidDistances.first;
    invalidConfig.maxDistance = invalidDistances.second;
    ThirdPersonCamera invalidCamera(invalidConfig);
    assert(invalidCamera.config().minDistance == builtInDefaults.minDistance);
    assert(invalidCamera.config().maxDistance == builtInDefaults.maxDistance);
    assert(invalidCamera.config().defaultDistance ==
           builtInDefaults.defaultDistance);
    assertReset(invalidCamera);
  }
}
