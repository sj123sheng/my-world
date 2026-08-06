#include "native/gameplay/player/exploration_motion.h"

#include <algorithm>
#include <cmath>

namespace {

bool finiteDt(float dtSeconds) {
  return std::isfinite(dtSeconds) && dtSeconds > 0.0f;
}

}  // namespace

ExplorationMotion::ExplorationMotion(ExplorationMotionConfig config)
    : config_(config) {
  if (config_.maxStamina <= 0.0f) config_.maxStamina = 100.0f;
  if (config_.jumpVelocity <= 0.0f) config_.jumpVelocity = 0.55f;
  if (config_.gravity <= 0.0f) config_.gravity = 1.7f;
  if (config_.glideFallSpeed <= 0.0f) config_.glideFallSpeed = 0.09f;
}

ExplorationMotionState ExplorationMotion::reset(float groundHeight) const {
  ExplorationMotionState state;
  state.state = MotionState::Grounded;
  state.height = std::isfinite(groundHeight) ? groundHeight : 0.0f;
  state.verticalVelocity = 0.0f;
  state.stamina = config_.maxStamina;
  state.regenDelaySeconds = 0.0f;
  return state;
}

ExplorationMotionState ExplorationMotion::update(
    const ExplorationMotionState& input, const MotionInput& motionInput,
    const TerrainHeightfield& terrain, float x, float y,
    float dtSeconds, const MotionGroundOverride* groundOverride) const {
  ExplorationMotionState state = input;
  if (!finiteDt(dtSeconds)) return state;
  // 地面高度：默认取地形采样；宿主层可覆盖为合成支撑面（建筑盒顶）。
  const float ground =
      (groundOverride != nullptr && groundOverride->active &&
       std::isfinite(groundOverride->groundHeight))
          ? groundOverride->groundHeight
          : terrain.heightAt(x, y);
  const bool inWater = terrain.waterAt(x, y);
  const float waterLevel = terrain.config().waterLevel;
  bool consumed = false;

  // 体力恢复：未消耗且延迟到期后线性恢复至上限。
  auto applyRegen = [&]() {
    if (consumed) {
      state.regenDelaySeconds = config_.staminaRegenDelaySeconds;
      return;
    }
    if (state.regenDelaySeconds > 0.0f) {
      state.regenDelaySeconds = std::max(0.0f, state.regenDelaySeconds - dtSeconds);
      return;
    }
    state.stamina = std::min(config_.maxStamina,
                             state.stamina + config_.staminaRegenPerSecond * dtSeconds);
  };

  switch (state.state) {
    case MotionState::Grounded: {
      // 贴地：地面高度连续跟随，避免悬空或陷入地形。
      state.height = ground;
      state.verticalVelocity = 0.0f;
      if (inWater) {
        // 走入水域：进入游泳，高度吸附到水面。
        state.state = MotionState::Swimming;
        state.height = waterLevel;
        break;
      }
      if (motionInput.jumpPressed && state.stamina >= config_.jumpStaminaCost) {
        state.stamina -= config_.jumpStaminaCost;
        state.verticalVelocity = config_.jumpVelocity;
        state.state = MotionState::Airborne;
        consumed = true;
        break;
      }
      // 陡坡/墙面攀爬：在可攀爬面上移动或正贴墙朝墙推进即进入攀爬，
      // 持续消耗体力；体力耗尽后退回站立，允许缓慢挪过陡坡但不能持续攀行。
      if (motionInput.moving &&
          (terrain.climbableAt(x, y) || motionInput.wallClimbing) &&
          state.stamina > 0.0f) {
        state.state = MotionState::Climbing;
        state.sprinting = false;
        break;
      }
      // 自动疾跑（探索优化）：地面持续移动达阈值后加速并消耗体力，
      // 体力耗尽或停步自动结束。
      if (motionInput.moving) {
        state.continuousMoveSeconds += dtSeconds;
      } else {
        state.continuousMoveSeconds = 0.0f;
      }
      state.sprinting = motionInput.moving &&
                        state.continuousMoveSeconds >=
                            config_.sprintActivateSeconds &&
                        state.stamina > 0.0f;
      if (state.sprinting) {
        state.stamina = std::max(
            0.0f, state.stamina - config_.sprintStaminaPerSecond * dtSeconds);
        consumed = true;
      }
      break;
    }
    case MotionState::Climbing: {
      const bool stillClimbing = motionInput.moving &&
                                 (terrain.climbableAt(x, y) ||
                                  motionInput.wallClimbing) &&
                                 !inWater && state.stamina > 0.0f;
      if (!stillClimbing) {
        state.state = inWater ? MotionState::Swimming : MotionState::Grounded;
        state.height = inWater ? waterLevel : ground;
        break;
      }
      if (motionInput.wallClimbing) {
        // 墙面攀爬：匀速上升；爬到盒顶后宿主层的地面覆盖会接管，
        // 切换到站立并站上墙头。地形攀爬仍沿坡面贴合地面。
        state.height = std::max(ground,
                                state.height + wallClimbSpeed() * dtSeconds);
      } else {
        state.height = ground;
      }
      state.stamina = std::max(0.0f, state.stamina -
                                         config_.climbStaminaPerSecond * dtSeconds);
      consumed = true;
      break;
    }
    case MotionState::Airborne:
    case MotionState::Gliding: {
      if (state.state == MotionState::Airborne && motionInput.glideHeld &&
          state.verticalVelocity < 0.0f && state.stamina > 0.0f) {
        state.state = MotionState::Gliding;
      }
      if (state.state == MotionState::Gliding &&
          (!motionInput.glideHeld || state.stamina <= 0.0f)) {
        state.state = MotionState::Airborne;
      }
      if (state.state == MotionState::Gliding) {
        // 滑翔：下落速度钳制到滑翔速度，体力持续消耗。
        state.verticalVelocity =
            std::max(state.verticalVelocity - config_.gravity * dtSeconds,
                     -config_.glideFallSpeed);
        state.stamina = std::max(0.0f, state.stamina -
                                           config_.glideStaminaPerSecond * dtSeconds);
        consumed = true;
      } else {
        state.verticalVelocity -= config_.gravity * dtSeconds;
      }
      state.height += state.verticalVelocity * dtSeconds;
      // 落水优先于落地判定：坠入水域直接进入游泳。
      if (inWater && state.height <= waterLevel) {
        state.state = MotionState::Swimming;
        state.height = waterLevel;
        state.verticalVelocity = 0.0f;
        break;
      }
      if (state.height <= ground) {
        state.state = MotionState::Grounded;
        state.height = ground;
        state.verticalVelocity = 0.0f;
      }
      break;
    }
    case MotionState::Swimming: {
      state.height = waterLevel;
      state.verticalVelocity = 0.0f;
      if (!inWater) {
        state.state = MotionState::Grounded;
        state.height = ground;
        break;
      }
      // 游泳缓慢消耗体力；水中起跳视为踩水上跳，成本同跳跃。
      state.stamina = std::max(0.0f, state.stamina -
                                         config_.swimStaminaPerSecond * dtSeconds);
      consumed = true;
      if (motionInput.jumpPressed && state.stamina >= config_.jumpStaminaCost) {
        state.stamina -= config_.jumpStaminaCost;
        state.verticalVelocity = config_.jumpVelocity * 0.7f;
        state.state = MotionState::Airborne;
      }
      break;
    }
  }

  applyRegen();
  state.stamina = std::clamp(state.stamina, 0.0f, config_.maxStamina);
  // 非地面状态不疾跑；疾跑中体力归零立即退出。
  if (state.state != MotionState::Grounded || state.stamina <= 0.0f) {
    if (state.state != MotionState::Grounded) {
      state.continuousMoveSeconds = 0.0f;
    }
    state.sprinting = false;
  }
  if (!std::isfinite(state.height)) state.height = ground;
  if (!std::isfinite(state.verticalVelocity)) state.verticalVelocity = 0.0f;
  return state;
}
