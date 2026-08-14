#pragma once

#include "engine/core/tick_clock.h"
#include "engine/math/vec2.h"

#include <array>
#include <cstdint>

enum class BossPhase : uint8_t {
  RadianceLockdown = 1,
  CurrentStorm = 2,
  CorruptionCollapse = 3,
};

enum class BossMechanic : uint8_t {
  None = 0,
  JudgmentBeam = 1,
  CurrentNodes = 2,
  FinalForge = 3,
};

struct BossConfig {
  FixedPoint maxHp = fp(1000);
  FixedPoint maxPoise = fp(300);
  FixedPoint phaseTwoHp = fp(700);
  FixedPoint phaseThreeHp = fp(350);
  Tick finalForgeCastMs = 5000;
  // 审判光束周期性施法：吟唱时长与两次施法间隔，
  // 让首领在血量阈值机制之外持续向玩家施压。
  Tick judgmentBeamCastMs = 1500;
  Tick judgmentBeamCooldownMs = 4000;
  // 开战后首次施法的延迟，给玩家进入状态的缓冲。
  Tick judgmentBeamFirstDelayMs = 2500;
  // 审判光束落地对玩家的伤害（可用闪避无敌帧规避）。
  FixedPoint judgmentBeamDamage = fp(15);
  FixedPoint judgmentBeamPoiseDamage = fp(10);
  uint8_t currentNodeCount = 2;

  // ---- 自由移动与普攻参数（与主角同源的持续追击/环绕走位）----
  Vec2 spawnPosition = {0.5f, 0.75f};
  // 移动速度（世界单位/秒）：略慢于主角 0.3，保持可拉扯感。
  float moveSpeedPerSecond = 0.22f;
  // 期望交战距离：小于该距离时切环绕走位，不再直冲。
  float preferredRange = 0.13f;
  // 环绕走位角速度（弧度/秒）。
  float orbitSpeedRadiansPerSecond = 1.1f;
  // 活动范围：以竞技场中心为圆心的钳制半径。
  Vec2 arenaCenter = {0.5f, 0.6f};
  float arenaRadius = 0.42f;

  // ---- 普攻循环：冷却归零且进入射程即起手，三变体轮换 ----
  Tick basicAttackFirstDelayMs = 1600;
  Tick basicAttackCooldownMs = 2400;
  Tick basicAttackWindupMs = 460;
  float basicAttackRange = 0.24f;
  FixedPoint basicAttackDamage = fp(12);
  FixedPoint basicAttackPoiseDamage = fp(8);

  static BossConfig karounDefaults();
  bool valid() const;
};

struct BossFrameInput {
  Tick tick = 0;
  int64_t dtMs = 0;
  bool resonanceAvailable = false;
  bool ultimateUsed = false;
  uint64_t sequence = 0;
  // 玩家实时位置与存活状态：驱动首领追击/环绕与普攻射程判定。
  Vec2 playerPosition;
  bool playerAlive = true;
};

struct BossSnapshot {
  BossPhase phase = BossPhase::RadianceLockdown;
  BossMechanic mechanic = BossMechanic::None;
  FixedPoint hp = fp(1000);
  FixedPoint poise = fp(300);
  bool vulnerable = true;
  bool failedMechanic = false;
  bool defeated = false;
  uint8_t transitionCount = 0;
  uint8_t nodesBroken = 0;
  Tick castRemainingMs = 0;
  Tick lastTransitionTick = 0;
  Tick retryTick = 0;
  // ---- 自由移动与普攻状态 ----
  // 首领实时位置/朝向：不再固定于 (0.5, 0.75)，随追击/环绕持续变化。
  Vec2 position = {0.5f, 0.75f};
  Vec2 facing{0.0f, -1.0f};
  bool moving = false;
  // 普攻前摇剩余毫秒：>0 表示正在蓄力，归零瞬间结算命中。
  Tick basicAttackCastRemainingMs = 0;
  // 普攻冷却剩余毫秒：归零且进入射程即起手下一次普攻。
  Tick basicAttackCooldownRemainingMs = 0;
  // 普攻变体（0/1/2 轮换）：驱动表现层差异化的挥击/束流/冲击波动效。
  uint8_t basicAttackVariant = 0;

  bool operator==(const BossSnapshot& other) const {
    return phase == other.phase && mechanic == other.mechanic &&
           hp == other.hp && poise == other.poise &&
           vulnerable == other.vulnerable &&
           failedMechanic == other.failedMechanic &&
           defeated == other.defeated &&
           transitionCount == other.transitionCount &&
           nodesBroken == other.nodesBroken &&
           castRemainingMs == other.castRemainingMs &&
           lastTransitionTick == other.lastTransitionTick &&
           retryTick == other.retryTick &&
           position == other.position && facing == other.facing &&
           moving == other.moving &&
           basicAttackCastRemainingMs == other.basicAttackCastRemainingMs &&
           basicAttackCooldownRemainingMs ==
               other.basicAttackCooldownRemainingMs &&
           basicAttackVariant == other.basicAttackVariant;
  }
};

class BossController {
 public:
  bool start(const BossConfig& config);
  bool retry(Tick tick);
  void applyDamage(FixedPoint hpDamage, FixedPoint poiseDamage, Tick tick);
  void update(const BossFrameInput& input);
  bool breakCurrentNode(uint8_t nodeIndex, Tick tick);

  const BossSnapshot& snapshot() const { return snapshot_; }
  const BossConfig& config() const { return config_; }

 private:
  void resetRuntime(Tick retryTick);
  void checkPhaseTransitions(Tick tick);
  void enterPhase(BossPhase phase, Tick tick);
  void refreshVulnerability();
  bool currentNodeBroken(uint8_t nodeIndex) const;
  // 自由移动（追击/环绕）与普攻起手，仅在无固定机制吟唱时执行。
  void updateMotionAndBasicAttack(const BossFrameInput& input, Tick elapsed);
  // 普攻计时（前摇/冷却）：独立于机制吟唱持续推进。
  void updateBasicAttackTimers(Tick elapsed);
  Vec2 clampInsideArena(Vec2 position) const;

  BossConfig config_ = BossConfig::karounDefaults();
  BossSnapshot snapshot_;
  bool running_ = false;
  bool phaseTwoTriggered_ = false;
  bool phaseThreeTriggered_ = false;
  // 审判光束施法冷却：归零时开始下一次吟唱。
  Tick castCooldownMs_ = 0;
  std::array<bool, 2> currentNodesBroken_{{false, false}};
  // 环绕走位方向（+1/-1）：每次普攻起手翻转，避免轨迹可预测。
  float orbitDirection_ = 1.0f;
};
