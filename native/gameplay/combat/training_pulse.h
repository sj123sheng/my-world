#pragma once

#include "combat_config.h"

#include <cstdint>

enum class PulseEventKind : uint8_t { None, Warning, Hit };
enum class DodgeGrade : uint8_t { Normal, Precise };

struct PulseEvent {
  PulseEventKind kind = PulseEventKind::None;
  Tick tick = 0;
};

class TrainingPulse {
 public:
  explicit TrainingPulse(CombatConfig config);

  PulseEvent advance(Tick now);
  DodgeGrade classifyDodge(Tick tick) const;

 private:
  CombatConfig config_;
  Tick lastAdvanceTick_ = -1;
};
