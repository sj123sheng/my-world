#include "native/engine/render/camera_render_state.h"
#include "native/gameplay/player/player_controller.h"
#include "native/gameplay/targeting/soft_targeting.h"

#include <cassert>
#include <cmath>
#include <vector>

namespace {

bool close(float actual, float expected) {
  return std::abs(actual - expected) < 0.0001f;
}

Vec2 difference(Vec2 lhs, Vec2 rhs) { return lhs - rhs; }

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
  assert(close(yawedPoint.x, 0.35f));
  assert(close(yawedPoint.y, 0.75f));

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

  constexpr float width = 1080.0f;
  constexpr float height = 1920.0f;
  const Vec2 neutralBillboard =
      neutral.billboardNdcRadii(0.04f, height / width);
  const Vec2 pitchedBillboard =
      pitched.billboardNdcRadii(0.04f, height / width);
  assert(close(neutralBillboard.x * width,
               neutralBillboard.y * height));
  assert(neutralBillboard == pitchedBillboard);
  const Vec2 distantBillboard =
      distant.billboardNdcRadii(0.04f, height / width);
  assert(distantBillboard.x < neutralBillboard.x);
  assert(distantBillboard.y < neutralBillboard.y);

  const std::vector<float> yaws{0.0f, 0.37f, 1.2f, -2.1f};
  const Vec2 move{0.6f, 0.8f};
  for (const float yaw : yaws) {
    Player player;
    PlayerController controller({1.0f, 100.0f});
    controller.update(player, move, yaw, 0.1f);

    const CameraRenderState state({0.5f, 0.5f}, yaw, 0.45f, 0.35f,
                                  0.45f, 0.35f);
    const Vec2 viewMove = difference(
        state.worldToView({player.x, player.y}),
        state.worldToView({0.5f, 0.5f}));
    assert(close(viewMove.x, move.x * 0.1f));
    assert(close(viewMove.y, move.y * 0.1f));

    const Vec2 worldFacing{std::cos(player.angle), std::sin(player.angle)};
    const Vec2 viewFacing = state.worldVectorToView(worldFacing);
    assert(close(std::atan2(viewFacing.y, viewFacing.x),
                 std::atan2(move.y, move.x)));

    const Vec2 forward{std::sin(yaw), std::cos(yaw)};
    const Vec2 forwardCandidate{0.5f + forward.x * 0.2f,
                                0.5f + forward.y * 0.2f};
    SoftTargeting targeting;
    const auto selected = targeting.select(
        {0.5f, 0.5f}, yaw, {{1, forwardCandidate}});
    assert(selected && selected->id == 1);
    const Vec2 forwardInView = state.worldToView(forwardCandidate);
    assert(close(forwardInView.x, 0.5f));
    assert(forwardInView.y > 0.5f);
  }
}
