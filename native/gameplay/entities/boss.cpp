#include "boss.h"

#include <algorithm>
#include <cmath>

BossConfig BossConfig::karounDefaults() { return {}; }

bool BossConfig::valid() const {
  return maxHp > 0 && maxPoise > 0 && phaseTwoHp > phaseThreeHp &&
         phaseTwoHp < maxHp && phaseThreeHp > 0 && finalForgeCastMs > 0 &&
         judgmentBeamCastMs > 0 && judgmentBeamCooldownMs > 0 &&
         currentNodeCount > 0 && currentNodeCount <= 2 &&
         moveSpeedPerSecond > 0.0f && preferredRange > 0.0f &&
         arenaRadius > 0.0f && basicAttackCooldownMs > 0 &&
         basicAttackWindupMs > 0 && basicAttackRange > 0.0f &&
         basicAttackDamage > 0;
}

bool BossController::start(const BossConfig& config) {
  if (!config.valid()) return false;
  config_ = config;
  running_ = true;
  resetRuntime(0);
  return true;
}

bool BossController::retry(Tick tick) {
  if (!running_) return false;
  resetRuntime(tick);
  return true;
}

void BossController::resetRuntime(Tick retryTick) {
  phaseTwoTriggered_ = false;
  phaseThreeTriggered_ = false;
  currentNodesBroken_ = {{false, false}};
  // 首次施法延迟，避免开战即吟唱。
  castCooldownMs_ = config_.judgmentBeamFirstDelayMs;
  orbitDirection_ = 1.0f;
  snapshot_ = {};
  snapshot_.phase = BossPhase::RadianceLockdown;
  snapshot_.mechanic = BossMechanic::None;
  snapshot_.hp = config_.maxHp;
  snapshot_.poise = config_.maxPoise;
  snapshot_.vulnerable = true;
  snapshot_.retryTick = retryTick;
  // 移动与普攻初始状态：从出生点起步，首次普攻延迟入场。
  snapshot_.position = config_.spawnPosition;
  snapshot_.facing = {0.0f, -1.0f};
  snapshot_.moving = false;
  snapshot_.basicAttackCastRemainingMs = 0;
  snapshot_.basicAttackCooldownRemainingMs = config_.basicAttackFirstDelayMs;
  snapshot_.basicAttackVariant = 0;
}

void BossController::applyDamage(FixedPoint hpDamage, FixedPoint poiseDamage,
                                 Tick tick) {
  if (!running_ || snapshot_.defeated || hpDamage < 0 || poiseDamage < 0) {
    return;
  }
  if (snapshot_.vulnerable && hpDamage > 0) {
    snapshot_.hp = std::max<FixedPoint>(0, snapshot_.hp - hpDamage);
  }
  if (poiseDamage > 0) {
    snapshot_.poise = std::max<FixedPoint>(0, snapshot_.poise - poiseDamage);
  }
  if (snapshot_.hp == 0) {
    snapshot_.defeated = true;
    snapshot_.mechanic = BossMechanic::None;
    snapshot_.castRemainingMs = 0;
    snapshot_.vulnerable = false;
    return;
  }
  checkPhaseTransitions(tick);
}

void BossController::update(const BossFrameInput& input) {
  if (!running_ || snapshot_.defeated) return;
  checkPhaseTransitions(input.tick);
  const Tick elapsed = input.dtMs <= 0 ? 0 : static_cast<Tick>(input.dtMs);

  // 终锻（三阶段机制）：玩家需在吟唱内用共鸣终结技打断。
  if (snapshot_.phase == BossPhase::CorruptionCollapse &&
      snapshot_.mechanic == BossMechanic::FinalForge &&
      snapshot_.castRemainingMs > 0 && !snapshot_.failedMechanic) {
    if (input.resonanceAvailable && input.ultimateUsed) {
      snapshot_.mechanic = BossMechanic::None;
      snapshot_.castRemainingMs = 0;
      snapshot_.vulnerable = true;
      return;
    }
    if (elapsed >= snapshot_.castRemainingMs) {
      snapshot_.castRemainingMs = 0;
      snapshot_.failedMechanic = true;
      snapshot_.vulnerable = true;
    } else {
      snapshot_.castRemainingMs -= elapsed;
    }
    return;
  }

  // 审判光束周期性施法：固定机制（电流节点/终锻）之外，
  // 首领持续吟唱施压，预警环与吟唱条给玩家应对窗口。
  if (snapshot_.mechanic == BossMechanic::None) {
    if (castCooldownMs_ > elapsed) {
      castCooldownMs_ -= elapsed;
    } else {
      snapshot_.mechanic = BossMechanic::JudgmentBeam;
      snapshot_.castRemainingMs = config_.judgmentBeamCastMs;
    }
  } else if (snapshot_.mechanic == BossMechanic::JudgmentBeam) {
    if (elapsed >= snapshot_.castRemainingMs) {
      snapshot_.castRemainingMs = 0;
      snapshot_.mechanic = BossMechanic::None;
      castCooldownMs_ = config_.judgmentBeamCooldownMs;
    } else {
      snapshot_.castRemainingMs -= elapsed;
    }
  }

  // 普攻计时独立于机制吟唱推进：机制期间只暂停走位与起手，
  // 节奏不被审判光束等固定机制冻结。
  updateBasicAttackTimers(elapsed);

  // 机制吟唱期间站桩施压；无机制时进入自由移动 + 普攻循环。
  if (snapshot_.mechanic == BossMechanic::None) {
    updateMotionAndBasicAttack(input, elapsed);
  } else {
    snapshot_.moving = false;
  }
}

void BossController::updateBasicAttackTimers(Tick elapsed) {
  // 普攻前摇推进：归零瞬间即命中帧，由遭遇层检测下降沿结算伤害。
  if (snapshot_.basicAttackCastRemainingMs > 0) {
    if (elapsed >= snapshot_.basicAttackCastRemainingMs) {
      snapshot_.basicAttackCastRemainingMs = 0;
      snapshot_.basicAttackCooldownRemainingMs =
          config_.basicAttackCooldownMs;
    } else {
      snapshot_.basicAttackCastRemainingMs -= elapsed;
    }
    return;
  }
  if (snapshot_.basicAttackCooldownRemainingMs > 0) {
    snapshot_.basicAttackCooldownRemainingMs =
        elapsed >= snapshot_.basicAttackCooldownRemainingMs
            ? 0
            : snapshot_.basicAttackCooldownRemainingMs - elapsed;
  }
}

void BossController::updateMotionAndBasicAttack(const BossFrameInput& input,
                                                Tick elapsed) {
  snapshot_.moving = false;
  // 朝向始终跟随玩家：保持面对面对峙姿态。
  const Vec2 delta = input.playerPosition - snapshot_.position;
  const float distance = delta.length();
  if (delta.finite() && std::isfinite(distance) && distance > 1e-4f) {
    snapshot_.facing = delta * (1.0f / distance);
  }

  // 前摇期间站桩蓄力，不移动也不起新招。
  if (snapshot_.basicAttackCastRemainingMs > 0) return;

  if (!input.playerAlive || !input.playerPosition.finite() ||
      !std::isfinite(distance)) {
    return;
  }

  // 普攻起手：冷却归零且玩家进入射程；起手瞬间翻转环绕方向，
  // 让走位轨迹不可预测。
  if (snapshot_.basicAttackCooldownRemainingMs <= 0 &&
      distance <= config_.basicAttackRange) {
    snapshot_.basicAttackCastRemainingMs = config_.basicAttackWindupMs;
    snapshot_.basicAttackVariant =
        static_cast<uint8_t>((snapshot_.basicAttackVariant + 1) % 3);
    orbitDirection_ = -orbitDirection_;
    return;
  }

  // 自由移动：远距离直线追击，到达期望距离后绕玩家环绕走位，
  // 与主角一样持续位移，不再固定站桩。
  if (distance <= 0.0f) return;
  const float dtSeconds = static_cast<float>(elapsed) / 1000.0f;
  Vec2 step{};
  if (distance > config_.preferredRange) {
    const float advance = std::min(
        config_.moveSpeedPerSecond * dtSeconds,
        distance - config_.preferredRange * 0.5f);
    step = snapshot_.facing * advance;
  } else {
    const Vec2 tangent{-snapshot_.facing.y * orbitDirection_,
                       snapshot_.facing.x * orbitDirection_};
    const float arc = config_.orbitSpeedRadiansPerSecond * dtSeconds *
                      config_.preferredRange;
    step = tangent * arc;
    // 贴得过近时向外侧微推，维持交战间距。
    if (distance < config_.preferredRange * 0.6f) {
      step = step - snapshot_.facing *
                        (config_.preferredRange * 0.6f - distance);
    }
  }
  if (step.finite() && step.length() > 0.0f) {
    snapshot_.position = clampInsideArena(snapshot_.position + step);
    snapshot_.moving = true;
  }
}

Vec2 BossController::clampInsideArena(Vec2 position) const {
  if (!position.finite()) return config_.spawnPosition;
  const Vec2 delta = position - config_.arenaCenter;
  const float distance = delta.length();
  if (!std::isfinite(distance) || distance <= config_.arenaRadius) {
    return position;
  }
  return config_.arenaCenter + delta * (config_.arenaRadius / distance);
}

bool BossController::breakCurrentNode(uint8_t nodeIndex, Tick tick) {
  if (!running_ || snapshot_.phase != BossPhase::CurrentStorm ||
      snapshot_.defeated || nodeIndex >= config_.currentNodeCount ||
      currentNodeBroken(nodeIndex)) {
    return false;
  }
  currentNodesBroken_[nodeIndex] = true;
  snapshot_.nodesBroken += 1;
  snapshot_.lastTransitionTick = tick;
  refreshVulnerability();
  return true;
}

void BossController::checkPhaseTransitions(Tick tick) {
  if (!phaseTwoTriggered_ && snapshot_.hp < config_.phaseTwoHp) {
    enterPhase(BossPhase::CurrentStorm, tick);
    phaseTwoTriggered_ = true;
  }
  if (!phaseThreeTriggered_ && snapshot_.hp < config_.phaseThreeHp) {
    enterPhase(BossPhase::CorruptionCollapse, tick);
    phaseThreeTriggered_ = true;
  }
}

void BossController::enterPhase(BossPhase phase, Tick tick) {
  snapshot_.phase = phase;
  snapshot_.lastTransitionTick = tick;
  snapshot_.transitionCount += 1;
  switch (phase) {
    case BossPhase::RadianceLockdown:
      snapshot_.mechanic = BossMechanic::None;
      break;
    case BossPhase::CurrentStorm:
      currentNodesBroken_ = {{false, false}};
      snapshot_.nodesBroken = 0;
      snapshot_.mechanic = BossMechanic::CurrentNodes;
      break;
    case BossPhase::CorruptionCollapse:
      snapshot_.mechanic = BossMechanic::FinalForge;
      snapshot_.castRemainingMs = config_.finalForgeCastMs;
      break;
  }
  refreshVulnerability();
}

void BossController::refreshVulnerability() {
  if (snapshot_.defeated) {
    snapshot_.vulnerable = false;
    return;
  }
  if (snapshot_.phase == BossPhase::CurrentStorm) {
    snapshot_.vulnerable = snapshot_.nodesBroken >= config_.currentNodeCount;
    return;
  }
  snapshot_.vulnerable = true;
}

bool BossController::currentNodeBroken(uint8_t nodeIndex) const {
  return nodeIndex < currentNodesBroken_.size() && currentNodesBroken_[nodeIndex];
}
