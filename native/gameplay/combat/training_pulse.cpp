#include "training_pulse.h"

#include <algorithm>
#include <cstdlib>
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
  if (tick < epochTick_) return DodgeGrade::Normal;
  const __int128 elapsed = static_cast<__int128>(tick) - epochTick_;
  const __int128 firstHit = config_.trainingPulseWarningMs;
  const __int128 window = config_.preciseDodgeWindowMs;
  if (elapsed + window < firstHit) return DodgeGrade::Normal;

  const __int128 period = config_.trainingPulsePeriodMs;
  const __int128 fromFirstHit = elapsed - firstHit;
  if (fromFirstHit < 0) {
    return -fromFirstHit <= window ? DodgeGrade::Precise : DodgeGrade::Normal;
  }
  const __int128 remainder = fromFirstHit % period;
  const __int128 distance = std::min(remainder, period - remainder);
  return distance <= window ? DodgeGrade::Precise : DodgeGrade::Normal;
}

Tick TrainingPulse::warningRemainingMs(Tick now) const {
  Tick warning = nextWarningTick_;
  if (warning <= now) {
    const Tick elapsed = now - warning;
    const Tick elapsedInPeriod = elapsed % config_.trainingPulsePeriodMs;
    return config_.trainingPulsePeriodMs - elapsedInPeriod;
  }
  return warning - now;
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
