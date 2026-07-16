#include "training_pulse.h"

#include <limits>

namespace { Tick advanceCursor(Tick value, Tick period) {
  const Tick maximum = std::numeric_limits<Tick>::max();
  return period > 0 && value > maximum - period ? maximum : value + period;
} }

TrainingPulse::TrainingPulse(CombatConfig config) : config_(config.validated()) {}

std::vector<PulseEvent> TrainingPulse::advance(Tick now) {
  std::vector<PulseEvent> events;
  if (now <= lastAdvanceTick_) return events;
  lastAdvanceTick_ = now;
  if (nextHitTick_ == 0) nextHitTick_ = config_.trainingPulseWarningMs;

  while (nextWarningTick_ <= now || nextHitTick_ <= now) {
    if (nextWarningTick_ <= nextHitTick_ && nextWarningTick_ <= now) {
      events.push_back({PulseEventKind::Warning, nextWarningTick_});
      const Tick prior = nextWarningTick_;
      nextWarningTick_ = advanceCursor(prior, config_.trainingPulsePeriodMs);
      if (nextWarningTick_ == prior) break;
    } else if (nextHitTick_ <= now) {
      events.push_back({PulseEventKind::Hit, nextHitTick_});
      const Tick prior = nextHitTick_;
      nextHitTick_ = advanceCursor(prior, config_.trainingPulsePeriodMs);
      if (nextHitTick_ == prior) break;
    }
  }
  return events;
}

DodgeGrade TrainingPulse::classifyDodge(Tick tick) const {
  return preciseDodgeHitTick(tick) ? DodgeGrade::Precise : DodgeGrade::Normal;
}

std::optional<Tick> TrainingPulse::preciseDodgeHitTick(Tick tick) const {
  if (tick < epochTick_) return std::nullopt;
  const __int128 elapsed = static_cast<__int128>(tick) - epochTick_;
  const __int128 firstHit = config_.trainingPulseWarningMs;
  const __int128 period = config_.trainingPulsePeriodMs;
  __int128 hitOffset = firstHit;
  if (elapsed >= firstHit) {
    hitOffset += ((elapsed - firstHit) / period + 1) * period;
  }
  const __int128 remaining = hitOffset - elapsed;
  if (remaining < config_.preciseDodgeWindowMinMs ||
      remaining > config_.preciseDodgeWindowMaxMs) {
    return std::nullopt;
  }
  const __int128 hitTick = static_cast<__int128>(epochTick_) + hitOffset;
  if (hitTick > std::numeric_limits<Tick>::max()) return std::nullopt;
  return static_cast<Tick>(hitTick);
}

Tick TrainingPulse::hitRemainingMs(Tick now) const {
  if (now < epochTick_) return config_.trainingPulseWarningMs;
  const __int128 elapsed = static_cast<__int128>(now) - epochTick_;
  const __int128 firstHit = config_.trainingPulseWarningMs;
  if (elapsed < firstHit) return static_cast<Tick>(firstHit - elapsed);
  const __int128 elapsedSinceHit = elapsed - firstHit;
  const __int128 remainder = elapsedSinceHit % config_.trainingPulsePeriodMs;
  return static_cast<Tick>(config_.trainingPulsePeriodMs - remainder);
}

PulseEventKind TrainingPulse::phase(Tick now) const {
  const Tick period = config_.trainingPulsePeriodMs;
  const Tick elapsed = now >= epochTick_ ? now - epochTick_ : 0;
  const Tick inCycle = elapsed % period;
  if (inCycle == config_.trainingPulseWarningMs) return PulseEventKind::Hit;
  if (inCycle < config_.trainingPulseWarningMs) return PulseEventKind::Warning;
  return PulseEventKind::None;
}

void TrainingPulse::resetAt(Tick now) {
  epochTick_ = now;
  lastAdvanceTick_ = now;
  nextWarningTick_ = advanceCursor(now, config_.trainingPulsePeriodMs);
  nextHitTick_ = advanceCursor(now, config_.trainingPulseWarningMs);
}
