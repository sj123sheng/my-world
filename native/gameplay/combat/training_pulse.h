#pragma once

#include "combat_config.h"

#include <cstdint>
#include <vector>

enum class PulseEventKind : uint8_t { None, Warning, Hit };
enum class DodgeGrade : uint8_t { Normal, Precise };

struct PulseEvent {
  PulseEventKind kind = PulseEventKind::None;
  Tick tick = 0;
};

class TrainingPulse {
 public:
  explicit TrainingPulse(CombatConfig config);

  std::vector<PulseEvent> advance(Tick now);
  DodgeGrade classifyDodge(Tick tick) const;
  Tick warningRemainingMs(Tick now) const;

 private:
  CombatConfig config_;
  Tick lastAdvanceTick_ = -1;
  Tick nextWarningTick_ = 0;
  Tick nextHitTick_ = 0;
};
