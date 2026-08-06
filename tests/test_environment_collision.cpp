// test_environment_collision.cpp: 建筑碰撞集与滑动解算回归测试。
// 覆盖：布局构建、OBB 推出与滑动、沿墙滑动保留切向、盒顶越过、
// 站立支撑查询、墙面攀爬与探索状态机集成。

#include "native/engine/world/environment_collision.h"

#include "native/gameplay/player/exploration_motion.h"

#include <cassert>
#include <cmath>

namespace {

// 世界中心附近一堵东西向横墙：中心 (0.5, 0.5)，半宽 0.1，半深 0.01，
// 盒顶高度 0.05，未旋转。
BuildingCollision singleWall() {
  BuildingBox box;
  box.cx = 0.5f;
  box.cz = 0.5f;
  box.hx = 0.1f;
  box.hz = 0.01f;
  box.yaw = 0.0f;
  box.top = 0.05f;
  return BuildingCollision{{box}};
}

}  // namespace

int main() {
  // ---- 布局构建：仅外圈与背景参与，盒体落在世界尺度内 ----
  const BuildingCollision layout = BuildingCollision::fromEnvironmentLayout(
      0.5f, 0.75f);
  assert(!layout.boxes().empty());
  int outerCount = 0;
  for (const BuildingBox& box : layout.boxes()) {
    // 盒体尺寸与位置均在世界尺度内（无米制残留）。
    assert(box.hx > 0.0f && box.hx < 0.2f);
    assert(box.hz > 0.0f && box.hz < 0.2f);
    assert(box.cx > -0.5f && box.cx < 1.5f);
    assert(box.cz > -0.5f && box.cz < 1.5f);
    assert(box.top > 0.0f && box.top < 0.3f);
    if (std::abs(box.cx - 0.5f) < 0.45f && std::abs(box.cz - 0.5f) < 0.45f) {
      ++outerCount;
    }
  }
  assert(outerCount > 0);  // 外圈城墙确实环绕可玩区。

  // ---- 推出与阻挡：墙内的点被推到盒外，墙外的点不动 ----
  const BuildingCollision wall = singleWall();
  {
    float x = 0.5f;
    float y = 0.505f;  // 盒内（半深 0.01）
    const BuildingContact contact = wall.resolve(x, y, 0.012f, 0.0f);
    assert(contact.touching);
    assert(std::abs(y - 0.5f) >= 0.01f + 0.012f - 1e-5f);  // 推到膨胀盒外
    assert(std::abs(x - 0.5f) < 1e-5f);                     // 法向推出无切向漂移
  }
  {
    float x = 0.5f;
    float y = 0.7f;  // 远离墙体
    const BuildingContact contact = wall.resolve(x, y, 0.012f, 0.0f);
    assert(!contact.touching);
    assert(x == 0.5f && y == 0.7f);
  }

  // ---- 沿墙滑动：从墙端部斜向侵入时，最浅轴为端部法向，
  // 另一轴分量保留形成沿墙滑动 ----
  {
    float x = 0.5f + 0.1f + 0.005f;  // 端部内侧（膨胀半宽 0.112）
    float y = 0.5f;
    const float startY = y;
    const BuildingContact contact = wall.resolve(x, y, 0.012f, 0.0f);
    assert(contact.touching);
    assert(x > 0.5f + 0.1f + 0.012f - 1e-5f);  // 推出膨胀盒
    assert(y == startY);                        // 切向（y）分量保留
  }

  // ---- 盒顶越过：高度高于盒顶时不阻挡（跳上/翻越）----
  {
    float x = 0.5f;
    float y = 0.505f;
    const BuildingContact contact = wall.resolve(x, y, 0.012f, 0.06f);
    assert(!contact.touching);
    assert(y == 0.505f);
  }

  // ---- 站立支撑：墙旁不误判，墙头顶上计入支撑 ----
  {
    // 贴墙站在地面：盒顶 0.05 远高于当前高度 0，不算支撑。
    const float beside =
        wall.standingTopAt(0.5f, 0.5f + 0.01f + 0.012f + 0.002f, 0.006f,
                           0.0f, 0.006f);
    assert(!std::isfinite(beside));
    // 站在盒顶（高度与盒顶齐平）：计入支撑。
    const float atop = wall.standingTopAt(0.5f, 0.5f, 0.006f, 0.05f, 0.006f);
    assert(std::isfinite(atop) && std::abs(atop - 0.05f) < 1e-6f);
  }

  // ---- 墙面攀爬集成：贴墙移动进入攀爬并抬升高度，耗体力 ----
  {
    TerrainConfig terrainConfig;
    terrainConfig.amplitude = 0.0f;
    terrainConfig.detailAmplitude = 0.0f;
    terrainConfig.ridgeAmplitude = 0.0f;
    terrainConfig.edgeMountainHeight = 0.0f;
    terrainConfig.waterLevel = -100.0f;
    terrainConfig.climbSlopeThreshold = 100.0f;
    const TerrainHeightfield flat{terrainConfig};
    const ExplorationMotion motion;

    ExplorationMotionState state = motion.reset(0.0f);
    MotionInput towardWall;
    towardWall.moving = true;
    towardWall.wallClimbing = true;
    state = motion.update(state, towardWall, flat, 0.5f, 0.45f, 0.016f);
    assert(state.state == MotionState::Climbing);
    // 过渡帧仅切状态；后续帧按固定速度抬升高度。
    for (int i = 0; i < 30; ++i) {
      state = motion.update(state, towardWall, flat, 0.5f, 0.45f, 0.016f);
    }
    assert(state.state == MotionState::Climbing);
    assert(state.height > 0.0f);
    const float climbed = state.height;
    for (int i = 0; i < 30; ++i) {
      state = motion.update(state, towardWall, flat, 0.5f, 0.45f, 0.016f);
    }
    assert(state.height > climbed);
    assert(state.stamina < motion.config().maxStamina);
    // 停止移动：退出攀爬回到站立。
    state = motion.update(state, MotionInput{}, flat, 0.5f, 0.45f, 0.016f);
    assert(state.state == MotionState::Grounded);
    // 体力耗尽不能进入攀爬。
    ExplorationMotionState exhausted = motion.reset(0.0f);
    exhausted.stamina = 0.0f;
    exhausted = motion.update(exhausted, towardWall, flat, 0.5f, 0.45f, 0.016f);
    assert(exhausted.state == MotionState::Grounded);
    assert(exhausted.height == 0.0f);
  }

  // ---- 地面覆盖：站上盒顶时地面取覆盖值 ----
  {
    TerrainConfig terrainConfig;
    terrainConfig.amplitude = 0.0f;
    terrainConfig.detailAmplitude = 0.0f;
    terrainConfig.ridgeAmplitude = 0.0f;
    terrainConfig.edgeMountainHeight = 0.0f;
    terrainConfig.waterLevel = -100.0f;
    const TerrainHeightfield flat{terrainConfig};
    const ExplorationMotion motion;
    ExplorationMotionState state = motion.reset(0.05f);
    MotionGroundOverride overrideGround;
    overrideGround.active = true;
    overrideGround.groundHeight = 0.05f;
    state = motion.update(state, MotionInput{}, flat, 0.5f, 0.5f, 0.016f,
                          &overrideGround);
    assert(state.state == MotionState::Grounded);
    assert(std::abs(state.height - 0.05f) < 1e-6f);
    // 从盒顶起跳后落回覆盖地面。
    MotionInput jump;
    jump.jumpPressed = true;
    state = motion.update(state, jump, flat, 0.5f, 0.5f, 0.016f,
                          &overrideGround);
    assert(state.state == MotionState::Airborne);
    for (int i = 0; i < 300 && state.state == MotionState::Airborne; ++i) {
      state = motion.update(state, MotionInput{}, flat, 0.5f, 0.5f, 0.016f,
                            &overrideGround);
    }
    assert(state.state == MotionState::Grounded);
    assert(std::abs(state.height - 0.05f) < 1e-4f);
  }

  return 0;
}
