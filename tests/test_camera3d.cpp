#include "native/engine/render/camera3d.h"

#include <cassert>
#include <cmath>

namespace {

void testFollowSetsPositionAndTarget() {
  Camera3D cam;
  cam.aspectRatio = 1.0f;
  cam.follow({0.5f, 0.0f, 0.5f}, 0.0f, 0.5f, 2.0f);
  assert(cam.position.y > 0.0f);
  assert(cam.target.x == 0.5f);
  assert(cam.target.z == 0.5f);
}

void testViewMatrixIsLookAt() {
  Camera3D cam;
  cam.aspectRatio = 1.0f;
  cam.follow({0.5f, 0.0f, 0.5f}, 0.0f, 0.5f, 2.0f);
  auto vp = cam.viewProjection();
  assert(vp.length() == 4);
  for (int i = 0; i < vp.length(); i++) {
    assert(std::isfinite(vp[i].x));
    assert(std::isfinite(vp[i].y));
    assert(std::isfinite(vp[i].z));
    assert(std::isfinite(vp[i].w));
  }
}

void testProjectionRespondsToFov() {
  Camera3D cam;
  cam.aspectRatio = 1.0f;
  cam.fov = 45.0f;
  auto p1 = cam.projectionMatrix();
  cam.fov = 90.0f;
  auto p2 = cam.projectionMatrix();
  assert(p1 != p2);
}

void testFollowYawRotatesPositionAroundTarget() {
  Camera3D cam;
  cam.aspectRatio = 1.0f;
  cam.follow({0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 1.0f);
  // pitch=0 -> position on xz plane, distance 1.
  // yaw=0 -> (sin0*cos0, sin0, -cos0*cos0) = (0, 0, -1)
  assert(std::fabs(cam.position.x) < 0.0001f);
  assert(std::fabs(cam.position.y) < 0.0001f);
  assert(std::fabs(cam.position.z + 1.0f) < 0.0001f);

  cam.follow({0.0f, 0.0f, 0.0f}, 3.14159265f / 2.0f, 0.0f, 1.0f);
  // yaw=pi/2 -> (-1, 0, 0)：相机始终位于水平前向的反方向。
  assert(std::fabs(cam.position.x + 1.0f) < 0.0001f);
  assert(std::fabs(cam.position.z) < 0.0001f);
}

void testProjectionIsFinite() {
  Camera3D cam;
  cam.aspectRatio = 1.6f;
  auto p = cam.projectionMatrix();
  for (int i = 0; i < p.length(); i++) {
    for (int j = 0; j < 4; j++) {
      assert(std::isfinite(p[i][j]));
    }
  }
}

glm::vec2 projectGroundPoint(const Camera3D& cam, glm::vec3 point) {
  const glm::vec4 clip = cam.viewProjection() * glm::vec4(point, 1.0f);
  return {clip.x / clip.w, clip.y / clip.w};
}

void testCameraRelativeBasisMatchesRealProjection() {
  constexpr float kPi = 3.14159265358979323846f;
  const float yaws[] = {0.0f, 0.45f, kPi * 0.5f, -1.2f};
  const glm::vec3 target{0.0f, 0.0f, 0.0f};

  for (const float yaw : yaws) {
    Camera3D cam;
    cam.aspectRatio = 1.0f;
    cam.follow(target, yaw, 0.45f, 2.0f);

    const glm::vec2 center = projectGroundPoint(cam, target);
    const glm::vec2 forward{std::sin(yaw), std::cos(yaw)};
    const glm::vec2 right{-std::cos(yaw), std::sin(yaw)};
    const glm::vec2 projectedForward = projectGroundPoint(
        cam, target + glm::vec3(forward.x, 0.0f, forward.y) * 0.2f);
    const glm::vec2 projectedRight = projectGroundPoint(
        cam, target + glm::vec3(right.x, 0.0f, right.y) * 0.2f);

    assert(std::fabs(projectedForward.x - center.x) < 0.0001f);
    assert(projectedForward.y > center.y);
    assert(projectedRight.x > center.x);
    assert(std::fabs(projectedRight.y - center.y) < 0.0001f);
  }
}

}  // namespace

int main() {
  testFollowSetsPositionAndTarget();
  testViewMatrixIsLookAt();
  testProjectionRespondsToFov();
  testFollowYawRotatesPositionAroundTarget();
  testProjectionIsFinite();
  testCameraRelativeBasisMatchesRealProjection();
  return 0;
}
