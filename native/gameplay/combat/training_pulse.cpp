#include "training_pulse.h"

#include <algorithm>
#include <cstdlib>

TrainingPulse::TrainingPulse(CombatConfig config) : config_(config.validated()) {}

std::vector<PulseEvent> TrainingPulse::advance(Tick now) {
  std::vector<PulseEvent> events;
  if (now <= lastAdvanceTick_) return events;
  lastAdvanceTick_ = now;
  if (nextHitTick_ == 0) nextHitTick_ = config_.trainingPulseWarningMs;

  while (nextWarningTick_ <= now || nextHitTick_ <= now) {
    if (nextWarningTick_ <= nextHitTick_ && nextWarningTick_ <= now) {
      events.push_back({PulseEventKind::Warning, nextWarningTick_});
      nextWarningTick_ += config_.trainingPulsePeriodMs;
    } else if (nextHitTick_ <= now) {
      events.push_back({PulseEventKind::Hit, nextHitTick_});
      nextHitTick_ += config_.trainingPulsePeriodMs;
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
    warning += (elapsed / config_.trainingPulsePeriodMs + 1) *
               config_.trainingPulsePeriodMs;
  }
  return warning - now;
}
