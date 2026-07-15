#include "combat_resources.h"

#include <algorithm>

CombatResources::CombatResources(CombatConfig config) : config_(config.validated()) {
  reset();
}

void CombatResources::reset() {
  stamina_ = config_.maxStamina;
  now_ = 0;
  recoveryStartTick_ = 0;
  recoveredMs_ = 0;
  recoveryRemainder_ = 0;
  insightExpiresAt_ = 0;
  insightAvailable_ = false;
}

bool CombatResources::spendStamina(FixedPoint amount, Tick tick) {
  advance(tick);
  if (amount < 0 || stamina_ < amount) return false;
  stamina_ -= amount;
  recoveryStartTick_ = tick + config_.staminaRecoveryDelayMs;
  recoveredMs_ = 0;
  recoveryRemainder_ = 0;
  return true;
}

void CombatResources::advance(Tick now) {
  if (now < now_) return;
  now_ = now;
  if (stamina_ >= config_.maxStamina || now < recoveryStartTick_) return;

  const Tick eligibleMs = now - recoveryStartTick_ + 1;
  const Tick elapsedMs = eligibleMs - recoveredMs_;
  if (elapsedMs <= 0) return;
  recoveredMs_ = eligibleMs;

  const int64_t scaled = elapsedMs * config_.staminaRecoveryPerSecond + recoveryRemainder_;
  stamina_ = std::min(config_.maxStamina, stamina_ + scaled / 1000);
  recoveryRemainder_ = scaled % 1000;
}

void CombatResources::grantInsight(Tick tick) {
  advance(tick);
  insightAvailable_ = true;
  insightExpiresAt_ = tick + config_.insightDurationMs;
}

bool CombatResources::consumeInsight(Tick tick) {
  advance(tick);
  if (!insightAvailable_ || tick >= insightExpiresAt_) {
    insightAvailable_ = false;
    return false;
  }
  insightAvailable_ = false;
  return true;
}

Tick CombatResources::insightRemainingMs() const {
  if (!insightAvailable_ || now_ >= insightExpiresAt_) return 0;
  return insightExpiresAt_ - now_;
}
