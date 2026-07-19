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
  // yaw=0 -> (cos0*cos0, sin0, cos0*sin0) = (1, 0, 0)
  assert(std::fabs(cam.position.x - 1.0f) < 0.0001f);
  assert(std::fabs(cam.position.y) < 0.0001f);
  assert(std::fabs(cam.position.z) < 0.0001f);

  cam.follow({0.0f, 0.0f, 0.0f}, 3.14159265f / 2.0f, 0.0f, 1.0f);
  // yaw=pi/2 -> (0, 0, 1)
  assert(std::fabs(cam.position.x) < 0.0001f);
  assert(std::fabs(cam.position.z - 1.0f) < 0.0001f);
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

}  // namespace

int main() {
  testFollowSetsPositionAndTarget();
  testViewMatrixIsLookAt();
  testProjectionRespondsToFov();
  testFollowYawRotatesPositionAroundTarget();
  testProjectionIsFinite();
  return 0;
}
