#pragma once

#include "combat_config.h"

#include <cstdint>
#include <optional>
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
  std::optional<Tick> preciseDodgeHitTick(Tick tick) const;
  Tick warningRemainingMs(Tick now) const;
  PulseEventKind phase(Tick now) const;
  void resetAt(Tick now);

 private:
  CombatConfig config_;
  Tick lastAdvanceTick_ = -1;
  Tick nextWarningTick_ = 0;
  Tick nextHitTick_ = 0;
  Tick epochTick_ = 0;
};
