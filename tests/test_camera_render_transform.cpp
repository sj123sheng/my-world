#include "native/engine/render/camera_render_state.h"

#include <cassert>
#include <cmath>

namespace {

bool close(float actual, float expected) {
  return std::abs(actual - expected) < 0.0001f;
}

}  // namespace

int main() {
  const Vec2 worldPoint{0.75f, 0.65f};
  const CameraRenderState neutral({0.5f, 0.5f}, 0.0f, 0.45f, 0.35f,
                                  0.45f, 0.35f);
  const Vec2 unchanged = neutral.worldToView(worldPoint);
  assert(close(unchanged.x, worldPoint.x));
  assert(close(unchanged.y, worldPoint.y));

  const CameraRenderState yawed({0.5f, 0.5f}, 1.57079632679f, 0.45f,
                                0.35f, 0.45f, 0.35f);
  const Vec2 yawedPoint = yawed.worldToView(worldPoint);
  assert(close(yawedPoint.x, 0.65f));
  assert(close(yawedPoint.y, 0.25f));

  const CameraRenderState pitched({0.5f, 0.5f}, 0.0f, 0.0f, 0.35f,
                                  0.45f, 0.35f);
  const Vec2 pitchedPoint = pitched.worldToView(worldPoint);
  assert(close(pitchedPoint.x, worldPoint.x));
  assert(pitchedPoint.y > worldPoint.y);

  const CameraRenderState distant({0.5f, 0.5f}, 0.0f, 0.45f, 0.6f,
                                  0.45f, 0.35f);
  const Vec2 distantPoint = distant.worldToView(worldPoint);
  assert(distantPoint.x > 0.5f && distantPoint.x < worldPoint.x);
  assert(distantPoint.y > 0.5f && distantPoint.y < worldPoint.y);

  const Vec2 size = yawed.worldSizeToView({0.1f, 0.2f});
  assert(close(size.x, 0.1f));
  assert(close(size.y, 0.2f));
}
