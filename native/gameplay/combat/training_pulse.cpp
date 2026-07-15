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
  const Tick firstHit = config_.trainingPulseWarningMs;
  Tick cycle = (tick - firstHit) / config_.trainingPulsePeriodMs;
  if (tick < firstHit) --cycle;
  const Tick priorHit = firstHit + cycle * config_.trainingPulsePeriodMs;
  const Tick nextHit = priorHit + config_.trainingPulsePeriodMs;
  const Tick distance = std::min(std::llabs(tick - priorHit), std::llabs(nextHit - tick));
  return distance <= config_.preciseDodgeWindowMs ? DodgeGrade::Precise : DodgeGrade::Normal;
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
  const Tick inCycle = now >= 0 ? now % period : 0;
  if (inCycle == config_.trainingPulseWarningMs) return PulseEventKind::Hit;
  if (inCycle < config_.trainingPulseWarningMs) return PulseEventKind::Warning;
  return PulseEventKind::None;
}
