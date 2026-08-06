#include "native/gameplay/player/exploration_motion.h"

#include <cassert>
#include <cmath>

namespace {

// 平底测试地形：全域高度 0，无水域、无可攀爬面。
TerrainHeightfield flatTerrain() {
  TerrainConfig config;
  config.amplitude = 0.0f;
  config.detailAmplitude = 0.0f;
  config.ridgeAmplitude = 0.0f;
  config.edgeMountainHeight = 0.0f;
  config.waterLevel = -100.0f;  // 永不触水
  config.climbSlopeThreshold = 100.0f;  // 永不可攀爬
  return TerrainHeightfield{config};
}

// 陡坡地形：全域可攀爬、无水域。
TerrainHeightfield climbTerrain() {
  TerrainConfig config;
  config.amplitude = 0.0f;
  config.detailAmplitude = 0.0f;
  config.ridgeAmplitude = 0.0f;
  config.edgeMountainHeight = 0.0f;
  config.waterLevel = -100.0f;
  config.climbSlopeThreshold = -1.0f;  // 坡度恒 >= 阈值
  return TerrainHeightfield{config};
}

// 水域地形用例直接在 main 内构造（水面高于地面）。

ExplorationMotionState step(const ExplorationMotion& motion,
                            ExplorationMotionState state,
                            const MotionInput& input,
                            const TerrainHeightfield& terrain,
                            float x, float y, float dt, int steps) {
  for (int i = 0; i < steps; ++i) {
    state = motion.update(state, input, terrain, x, y, dt);
  }
  return state;
}

}  // namespace

int main() {
  ExplorationMotion motion;
  const TerrainHeightfield flat = flatTerrain();
  const float dt = 0.016f;

  // 复位：贴地、满体力。
  ExplorationMotionState state = motion.reset(0.0f);
  assert(state.state == MotionState::Grounded);
  assert(state.height == 0.0f);
  assert(state.stamina == motion.config().maxStamina);

  // 静止站立：高度保持，体力保持满值。
  state = step(motion, state, {}, flat, 0.5f, 0.5f, dt, 10);
  assert(state.state == MotionState::Grounded);
  assert(state.height == 0.0f);
  assert(state.stamina == motion.config().maxStamina);

  // 跳跃：按下跳跃键进入空中，上升后受重力回落并最终落地。
  ExplorationMotionState jumping = motion.update(
      state, MotionInput{true, false, false}, flat, 0.5f, 0.5f, dt);
  assert(jumping.state == MotionState::Airborne);
  assert(jumping.verticalVelocity > 0.0f);
  assert(jumping.stamina < state.stamina);
  const float expectedStamina =
      state.stamina - motion.config().jumpStaminaCost;
  assert(std::abs(jumping.stamina - expectedStamina) < 0.001f);

  // 自由落体最终回到地面。
  ExplorationMotionState landed = step(
      motion, jumping, {}, flat, 0.5f, 0.5f, dt, 300);
  assert(landed.state == MotionState::Grounded);
  assert(std::abs(landed.height) < 0.001f);
  assert(landed.verticalVelocity == 0.0f);

  // 滑翔：下落段按住滑翔键，下落速度被钳制且体力持续消耗。
  ExplorationMotionState apex = jumping;
  MotionInput holdGlide{false, true, false};
  while (apex.verticalVelocity >= 0.0f) {
    apex = motion.update(apex, {}, flat, 0.5f, 0.5f, dt);
  }
  const float staminaBeforeGlide = apex.stamina;
  ExplorationMotionState gliding = motion.update(apex, holdGlide, flat,
                                                 0.5f, 0.5f, dt);
  assert(gliding.state == MotionState::Gliding);
  gliding = step(motion, gliding, holdGlide, flat, 0.5f, 0.5f, dt, 20);
  assert(gliding.state == MotionState::Gliding);
  assert(gliding.verticalVelocity >= -motion.config().glideFallSpeed - 0.001f);
  assert(gliding.stamina < staminaBeforeGlide);
  // 松开滑翔键回到自由落体。
  ExplorationMotionState falling = motion.update(gliding, {}, flat,
                                                 0.5f, 0.5f, dt);
  assert(falling.state == MotionState::Airborne);

  // 体力耗尽时不能起跳。
  ExplorationMotionState exhausted = motion.reset(0.0f);
  exhausted.stamina = motion.config().jumpStaminaCost - 1.0f;
  ExplorationMotionState noJump = motion.update(
      exhausted, MotionInput{true, false, false}, flat, 0.5f, 0.5f, dt);
  assert(noJump.state == MotionState::Grounded);
  assert(noJump.verticalVelocity == 0.0f);

  // 攀爬：可攀爬面上移动进入攀爬并持续消耗体力。
  const TerrainHeightfield climbable = climbTerrain();
  ExplorationMotionState climber = motion.reset(0.0f);
  MotionInput movingForward{false, false, true};
  ExplorationMotionState climbing = motion.update(
      climber, movingForward, climbable, 0.5f, 0.5f, dt);
  assert(climbing.state == MotionState::Climbing);
  climbing = step(motion, climbing, movingForward, climbable,
                  0.5f, 0.5f, dt, 20);
  assert(climbing.state == MotionState::Climbing);
  assert(climbing.stamina < motion.config().maxStamina);
  // 停止移动退出攀爬。
  ExplorationMotionState stopClimb = motion.update(climbing, {}, climbable,
                                                   0.5f, 0.5f, dt);
  assert(stopClimb.state == MotionState::Grounded);

  // 游泳：走入水域进入游泳，高度吸附水面；离开水域回到地面。
  TerrainConfig waterConfig;
  waterConfig.amplitude = 0.0f;
  waterConfig.detailAmplitude = 0.0f;
  waterConfig.ridgeAmplitude = 0.0f;
  waterConfig.edgeMountainHeight = 0.0f;
  waterConfig.waterLevel = 0.05f;  // 地面高度 0 < 0.05 → 全域水域
  waterConfig.climbSlopeThreshold = 100.0f;
  const TerrainHeightfield water{waterConfig};
  ExplorationMotionState swimmer = motion.reset(0.0f);
  ExplorationMotionState swimming = motion.update(swimmer, {}, water,
                                                  0.5f, 0.5f, dt);
  assert(swimming.state == MotionState::Swimming);
  assert(swimming.height == water.config().waterLevel);
  // 游泳缓慢消耗体力。
  swimming = step(motion, swimming, {}, water, 0.5f, 0.5f, dt, 30);
  assert(swimming.state == MotionState::Swimming);
  assert(swimming.stamina < motion.config().maxStamina);

  // 体力恢复：消耗停止并等待延迟后开始恢复。
  ExplorationMotionState regen = motion.reset(0.0f);
  regen.stamina = 50.0f;
  regen.regenDelaySeconds = 0.0f;
  ExplorationMotionState recovered = step(motion, regen, {}, flat,
                                          0.5f, 0.5f, dt, 60);
  assert(recovered.stamina > 50.0f);
  // 恢复延迟期内不恢复。
  ExplorationMotionState delayed = motion.reset(0.0f);
  delayed.stamina = 50.0f;
  delayed.regenDelaySeconds = 100.0f;
  ExplorationMotionState delayedAfter = step(motion, delayed, {}, flat,
                                             0.5f, 0.5f, dt, 10);
  assert(delayedAfter.stamina == 50.0f);

  // 非法 dt 不推进状态。
  ExplorationMotionState unchanged = motion.update(state, MotionInput{true, false, false},
                                                   flat, 0.5f, 0.5f, 0.0f);
  assert(unchanged.state == state.state);

  // 自动疾跑（探索优化）：地面持续移动 1.5 秒后激活，消耗体力；
  // 停步或体力耗尽自动退出。
  MotionInput walkInput;
  walkInput.moving = true;
  ExplorationMotionState sprint = motion.reset(0.0f);
  sprint = step(motion, sprint, walkInput, flat, 0.5f, 0.5f, 0.1f, 10);
  assert(!sprint.sprinting);  // 1.0 秒未达阈值。
  const float staminaBeforeSprint = sprint.stamina;
  sprint = step(motion, sprint, walkInput, flat, 0.5f, 0.5f, 0.1f, 6);
  assert(sprint.sprinting);   // 1.6 秒已激活。
  assert(sprint.stamina < staminaBeforeSprint);  // 疾跑消耗体力。
  // 停步立即退出疾跑并清零累计。
  sprint = motion.update(sprint, MotionInput{}, flat, 0.5f, 0.5f, 0.1f);
  assert(!sprint.sprinting);
  assert(sprint.continuousMoveSeconds == 0.0f);
  // 体力耗尽自动退出：手动压低体力后持续疾跑到归零。
  sprint = motion.reset(0.0f);
  sprint = step(motion, sprint, walkInput, flat, 0.5f, 0.5f, 0.1f, 16);
  assert(sprint.sprinting);
  sprint.stamina = 0.2f;
  sprint = motion.update(sprint, walkInput, flat, 0.5f, 0.5f, 0.1f);
  assert(sprint.stamina == 0.0f);
  assert(!sprint.sprinting);

  // 有限性：长时间模拟不产生 NaN。
  ExplorationMotionState longRun = motion.reset(0.0f);
  for (int i = 0; i < 10000; ++i) {
    MotionInput input;
    input.jumpPressed = (i % 50) == 0;
    input.glideHeld = (i / 50) % 2 == 1;
    input.moving = (i % 7) < 3;
    longRun = motion.update(longRun, input, flat, 0.5f, 0.5f, dt);
    assert(std::isfinite(longRun.height));
    assert(std::isfinite(longRun.verticalVelocity));
    assert(std::isfinite(longRun.stamina));
  }
  return 0;
}
