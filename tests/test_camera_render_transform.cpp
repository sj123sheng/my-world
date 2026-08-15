#include "native/engine/render/camera_render_state.h"
#include "native/engine/render/environment.h"
#include "native/engine/world/world_position.h"
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

void testChunkRenderTranslationAtExtremeOrigin() {
  // 极远原点（10^12）下相邻块平移必须有限且精确：先按 long double
  // 求整数分块差再转 float，避免先转 float 造成的精度塌缩。
  const ChunkCoord origin{1000000000000LL, -1000000000000LL};
  const LocalPosition local{0.75f, 0.25f};
  const glm::vec3 east =
      ChunkRenderTranslation({origin.x + 1, origin.y}, origin, local);
  assert(std::isfinite(east.x) && std::isfinite(east.y) &&
         std::isfinite(east.z));
  assert(close(east.x, 1.0f));
  assert(close(east.y, 0.0f));
  assert(close(east.z, 0.0f));

  const glm::vec3 southWest =
      ChunkRenderTranslation({origin.x - 2, origin.y + 3}, origin, local);
  assert(close(southWest.x, -2.0f));
  assert(close(southWest.z, 3.0f));

  // 玩家所在分块平移为零（渲染原点即原点分块角点）。
  const glm::vec3 self = ChunkRenderTranslation(origin, origin, local);
  assert(close(self.x, 0.0f) && close(self.y, 0.0f) && close(self.z, 0.0f));

  // 超远目标差值超出 float 安全整数范围时钳制为有限值。
  const glm::vec3 far = ChunkRenderTranslation(
      {origin.x + 9000000000000LL, origin.y}, origin, local);
  assert(std::isfinite(far.x) && std::isfinite(far.z));
}

void testChunkRenderCommitRespectsActiveRadius() {
  // 超出活动半径的目标块不提交 GPU 网格（切比雪夫距离判定）。
  const ChunkCoord origin{1000000000000LL, -1000000000000LL};
  assert(ChunkRenderCommittable(origin, origin, 4));
  assert(ChunkRenderCommittable({origin.x + 4, origin.y - 4}, origin, 4));
  assert(!ChunkRenderCommittable({origin.x + 5, origin.y}, origin, 4));
  assert(!ChunkRenderCommittable({origin.x, origin.y + 5}, origin, 4));
  assert(!ChunkRenderCommittable({origin.x - 5, origin.y - 5}, origin, 4));
  // 低画质半径收缩后判定同步收缩。
  assert(ChunkRenderCommittable({origin.x + 2, origin.y + 2}, origin, 2));
  assert(!ChunkRenderCommittable({origin.x + 3, origin.y}, origin, 2));
}

}  // namespace

int main() {
  testChunkRenderTranslationAtExtremeOrigin();
  testChunkRenderCommitRespectsActiveRadius();
  const Vec2 worldPoint{0.75f, 0.65f};
  const CameraRenderState neutral({0.5f, 0.5f}, 0.0f, 0.45f, 0.35f,
                                  0.45f, 0.35f);
  const Vec2 neutralPoint = neutral.worldToView(worldPoint);
  assert(close(neutralPoint.x, 1.0f - worldPoint.x));
  assert(close(neutralPoint.y, 1.0f - worldPoint.y));

  const CameraRenderState yawed({0.5f, 0.5f}, 1.57079632679f, 0.45f,
                                0.35f, 0.45f, 0.35f);
  const Vec2 yawedPoint = yawed.worldToView(worldPoint);
  assert(close(yawedPoint.x, 0.65f));
  assert(close(yawedPoint.y, 0.25f));

  const CameraRenderState pitched({0.5f, 0.5f}, 0.0f, 0.0f, 0.35f,
                                  0.45f, 0.35f);
  const Vec2 pitchedPoint = pitched.worldToView(worldPoint);
  assert(close(pitchedPoint.x, 1.0f - worldPoint.x));
  assert(pitchedPoint.y < neutralPoint.y);

  const CameraRenderState distant({0.5f, 0.5f}, 0.0f, 0.45f, 0.6f,
                                  0.45f, 0.35f);
  const Vec2 distantPoint = distant.worldToView(worldPoint);
  assert(distantPoint.x > neutralPoint.x && distantPoint.x < 0.5f);
  assert(distantPoint.y > neutralPoint.y && distantPoint.y < 0.5f);

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
    // 速度平滑需要数帧收敛到满速，取收敛后最后一帧的位移验证。
    Vec2 previous{player.x, player.y};
    for (int frame = 0; frame < 21; ++frame) {
      previous = {player.x, player.y};
      controller.update(player, move, yaw, 0.016f);
    }

    const CameraRenderState state({0.5f, 0.5f}, yaw, 0.45f, 0.35f,
                                  0.45f, 0.35f);
    const Vec2 viewMove = difference(
        state.worldToView({player.x, player.y}),
        state.worldToView(previous));
    assert(close(viewMove.x, move.x * 0.016f));
    assert(close(viewMove.y, -move.y * 0.016f));

    const Vec2 worldFacing{std::sin(player.angle), std::cos(player.angle)};
    const Vec2 viewFacing = state.worldVectorToView(worldFacing);
    assert(close(std::atan2(-viewFacing.y, viewFacing.x),
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
    assert(forwardInView.y < 0.5f);
  }
}
