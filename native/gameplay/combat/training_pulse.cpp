#include "training_pulse.h"

#include <algorithm>
#include <cstdlib>

TrainingPulse::TrainingPulse(CombatConfig config) : config_(config.validated()) {}

PulseEvent TrainingPulse::advance(Tick now) {
  if (now <= lastAdvanceTick_) return {};
  const Tick previous = lastAdvanceTick_;
  lastAdvanceTick_ = now;

  const Tick hitCycle = now >= config_.trainingPulseWarningMs
                            ? (now - config_.trainingPulseWarningMs) /
                                  config_.trainingPulsePeriodMs
                            : -1;
  const Tick hitTick = hitCycle >= 0
                           ? hitCycle * config_.trainingPulsePeriodMs +
                                 config_.trainingPulseWarningMs
                           : -1;
  const Tick warningTick = (now / config_.trainingPulsePeriodMs) *
                           config_.trainingPulsePeriodMs;
  if (hitTick > previous && hitTick >= warningTick) {
    return {PulseEventKind::Hit, hitTick};
  }
  if (warningTick > previous) return {PulseEventKind::Warning, warningTick};
  return {};
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
