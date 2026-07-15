#include "combat_resources.h"

#include <algorithm>
#include <limits>

namespace {

Tick saturatingAdd(Tick tick, Tick duration) {
  const Tick maximum = std::numeric_limits<Tick>::max();
  if (duration > 0 && tick > maximum - duration) return maximum;
  return tick + duration;
}

std::size_t sourceIndex(SourceType source) {
  switch (source) {
    case SourceType::Radiance: return 0;
    case SourceType::Current: return 1;
    case SourceType::Corruption: return 2;
  }
  return 0;
}

}  // namespace

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
  insightGrantedAt_ = 0;
  insightAvailable_ = false;
  sourceReadyAt_.fill(0);
  distinctSourceTicks_.fill(std::nullopt);
  lastDistinctSourceTick_.reset();
  resonance_ = 0;
  ultimateWindowExpiresAt_ = 0;
  ultimateWindowActive_ = false;
}

bool CombatResources::spendStamina(FixedPoint amount, Tick tick) {
  advance(tick);
  if (amount < 0 || stamina_ < amount) return false;
  stamina_ -= amount;
  recoveryStartTick_ = saturatingAdd(tick, config_.staminaRecoveryDelayMs);
  recoveredMs_ = 0;
  recoveryRemainder_ = 0;
  return true;
}

void CombatResources::advance(Tick now) {
  if (now < now_) return;
  now_ = now;
  if (ultimateWindowActive_ && now >= ultimateWindowExpiresAt_) {
    ultimateWindowActive_ = false;
  }
  if (stamina_ >= config_.maxStamina || now < recoveryStartTick_) return;

  const __int128 eligibleMs = static_cast<__int128>(now) - recoveryStartTick_ + 1;
  const __int128 elapsedMs = eligibleMs - recoveredMs_;
  if (elapsedMs <= 0) return;
  recoveredMs_ = eligibleMs;

  const __int128 scaled = elapsedMs *
                          config_.staminaRecoveryPerSecond + recoveryRemainder_;
  const __int128 gain = scaled / 1000;
  stamina_ = gain >= config_.maxStamina - stamina_
                 ? config_.maxStamina
                 : stamina_ + static_cast<FixedPoint>(gain);
  recoveryRemainder_ = stamina_ == config_.maxStamina
                           ? 0
                           : static_cast<int64_t>(scaled % 1000);
}

void CombatResources::grantInsight(Tick tick) {
  advance(tick);
  insightAvailable_ = true;
  insightGrantedAt_ = tick;
  insightExpiresAt_ = saturatingAdd(tick, config_.insightDurationMs);
}

bool CombatResources::consumeInsight(Tick tick) {
  advance(tick);
  if (!insightAvailableAt(tick)) {
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

bool CombatResources::hasInsight() const {
  return insightAvailable_ && now_ < insightExpiresAt_;
}

bool CombatResources::insightAvailableAt(Tick tick) const {
  return insightAvailable_ && tick >= insightGrantedAt_ && tick < insightExpiresAt_;
}

bool CombatResources::sourceReady(SourceType source, Tick tick) const {
  return tick >= sourceReadyAt_[sourceIndex(source)];
}

void CombatResources::startSourceCooldown(SourceType source, Tick tick) {
  const std::size_t index = sourceIndex(source);
  sourceReadyAt_[index] = saturatingAdd(tick, config_.sourceCooldownMs[index]);
}

void CombatResources::addResonance(FixedPoint amount) {
  if (amount <= 0) return;
  if (amount >= config_.maxResonance - resonance_) {
    resonance_ = config_.maxResonance;
    return;
  }
  resonance_ += amount;
}

void CombatResources::recordDistinctSource(SourceType source, Tick tick) {
  if (lastDistinctSourceTick_ && tick < *lastDistinctSourceTick_) return;
  lastDistinctSourceTick_ = tick;
  distinctSourceTicks_[sourceIndex(source)] = tick;
  for (auto& recorded : distinctSourceTicks_) {
    if (recorded && static_cast<__int128>(tick) - *recorded > config_.triSourceWindowMs) {
      recorded.reset();
    }
  }
  if (!distinctSourceTicks_[0] || !distinctSourceTicks_[1] || !distinctSourceTicks_[2]) return;

  const Tick minimum = std::min({*distinctSourceTicks_[0], *distinctSourceTicks_[1],
                                 *distinctSourceTicks_[2]});
  const Tick maximum = std::max({*distinctSourceTicks_[0], *distinctSourceTicks_[1],
                                 *distinctSourceTicks_[2]});
  if (static_cast<__int128>(maximum) - minimum <= config_.triSourceWindowMs) {
    resonance_ = config_.maxResonance;
    ultimateWindowActive_ = true;
    ultimateWindowExpiresAt_ = saturatingAdd(tick, config_.ultimateWindowMs);
    distinctSourceTicks_.fill(std::nullopt);
  }
}

bool CombatResources::canUltimate(Tick tick) {
  advance(tick);
  return resonance_ >= config_.maxResonance;
}

bool CombatResources::spendUltimate(Tick tick) {
  if (!canUltimate(tick)) return false;
  resonance_ = 0;
  ultimateWindowActive_ = false;
  ultimateWindowExpiresAt_ = 0;
  return true;
}

bool CombatResources::ultimateWindowActive(Tick tick) {
  advance(tick);
  return ultimateWindowActive_;
}

Tick CombatResources::sourceCooldownRemainingMs(SourceType source) const {
  const Tick ready = sourceReadyAt_[sourceIndex(source)];
  return ready > now_ ? ready - now_ : 0;
}

Tick CombatResources::ultimateWindowRemainingMs() const {
  return ultimateWindowActive_ && ultimateWindowExpiresAt_ > now_
             ? ultimateWindowExpiresAt_ - now_
             : 0;
}
