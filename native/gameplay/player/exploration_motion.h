#pragma once

#include "native/engine/world/terrain_heightfield.h"

#include <cstdint>

// 探索运动状态机：在平面移动（PlayerController）之上叠加垂直维度，
// 支撑开放世界探索的跳跃、攀爬、滑翔与游泳，并统一体力（stamina）
// 消耗与恢复。所有规则确定性、无外部随机，可被独立测试覆盖。
//
// 坐标系约定与地形模块一致：逻辑 (x, y) 对应 3D (x, height, z=y)。
enum class MotionState : uint8_t {
  Grounded = 0,   // 地面站立/奔跑
  Airborne = 1,   // 跳跃上升与下落
  Gliding = 2,    // 空中滑翔（按住滑翔键）
  Climbing = 3,   // 陡坡攀爬（持续消耗体力）
  Swimming = 4,   // 水面游泳
};

struct ExplorationMotionConfig {
  float maxStamina = 100.0f;
  // 起跳初速度（世界单位/秒，3D Y 方向）。
  float jumpVelocity = 0.55f;
  float gravity = 1.7f;
  // 滑翔时的匀速下落速度（远小于自由落体）。
  float glideFallSpeed = 0.09f;
  float jumpStaminaCost = 8.0f;
  float climbStaminaPerSecond = 16.0f;
  float glideStaminaPerSecond = 6.0f;
  float swimStaminaPerSecond = 2.5f;
  float staminaRegenPerSecond = 14.0f;
  // 停止消耗后的恢复延迟，避免频繁抖动消耗/恢复。
  float staminaRegenDelaySeconds = 0.7f;
  // 地面高度单帧向下台阶上限：超过即视为走离悬崖边缘，进入坠落
  // 而不是贴地瞬移（原神式走离台阶下落）。可行走坡度（< 攀爬阈值）
  // 在正常移速下的单帧下降远小于该值，不会误触发。
  float maxStepDown = 0.008f;
  // 地面高度突变时的最大贴地速度，避免跨块或高差边界在一帧内瞬移。
  float maxGroundFollowPerSecond = 0.25f;
  // 自动疾跑（原神式）：地面持续移动达阈值后加速并消耗体力。
  float sprintActivateSeconds = 1.5f;
  float sprintStaminaPerSecond = 9.0f;
  float sprintSpeedMultiplier = 1.5f;
};

struct ExplorationMotionState {
  MotionState state = MotionState::Grounded;
  // 角色脚底的 3D 高度。
  float height = 0.0f;
  float verticalVelocity = 0.0f;
  float stamina = 100.0f;
  float regenDelaySeconds = 0.0f;
  // 自动疾跑：地面连续移动累计秒数与当前疾跑状态。
  float continuousMoveSeconds = 0.0f;
  bool sprinting = false;
};

struct MotionInput {
  bool jumpPressed = false;
  bool glideHeld = false;
  // 水平方向是否有移动输入（攀爬判定需要角色正朝坡面移动）。
  bool moving = false;
  // 历史输入位保留为运动状态机二进制兼容；自然世界生产路径固定 false。
  bool wallClimbing = false;
  // 同上；Loop 的自然世界生产路径固定 false。
  bool terrainClimbing = false;
};

class ExplorationMotion {
 public:
  explicit ExplorationMotion(ExplorationMotionConfig config = {});

  // 复位到初始站立状态（体力满值、贴地）。
  ExplorationMotionState reset(float groundHeight) const;

  // 推进一个固定步。返回更新后的状态（值语义，便于测试断言）。
  ExplorationMotionState update(const ExplorationMotionState& state,
                                const MotionInput& input,
                                const TerrainHeightfield& terrain,
                                double x, double y, float dtSeconds) const;

  const ExplorationMotionConfig& config() const { return config_; }

  // 墙面攀爬上升速度（世界单位/秒）。
  static constexpr float wallClimbSpeed() { return 0.1f; }

 private:
  ExplorationMotionConfig config_;
};
