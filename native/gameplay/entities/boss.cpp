#include "boss.h"

#include <algorithm>

BossConfig BossConfig::karounDefaults() { return {}; }

bool BossConfig::valid() const {
  return maxHp > 0 && maxPoise > 0 && phaseTwoHp > phaseThreeHp &&
         phaseTwoHp < maxHp && phaseThreeHp > 0 && finalForgeCastMs > 0 &&
         judgmentBeamCastMs > 0 && judgmentBeamCooldownMs > 0 &&
         currentNodeCount > 0 && currentNodeCount <= 2;
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
  snapshot_ = {};
  snapshot_.phase = BossPhase::RadianceLockdown;
  snapshot_.mechanic = BossMechanic::None;
  snapshot_.hp = config_.maxHp;
  snapshot_.poise = config_.maxPoise;
  snapshot_.vulnerable = true;
  snapshot_.retryTick = retryTick;
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
