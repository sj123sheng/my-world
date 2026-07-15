#pragma once

#include "combat_config.h"
#include "event.h"

#include <array>
#include <optional>

class CombatResources {
 public:
  explicit CombatResources(CombatConfig config);

  bool spendStamina(FixedPoint amount, Tick tick);
  void advance(Tick now);
  void grantInsight(Tick tick);
  bool consumeInsight(Tick tick);
  bool hasInsight() const;
  bool sourceReady(SourceType source, Tick tick) const;
  void startSourceCooldown(SourceType source, Tick tick);
  void addResonance(FixedPoint amount);
  void recordDistinctSource(SourceType source, Tick tick);
  bool canUltimate(Tick tick);
  bool spendUltimate(Tick tick);
  bool ultimateWindowActive(Tick tick);
  void reset();

  FixedPoint stamina() const { return stamina_; }
  Tick insightRemainingMs() const;
  FixedPoint resonance() const { return resonance_; }

 private:
  CombatConfig config_;
  FixedPoint stamina_ = 0;
  Tick now_ = 0;
  Tick recoveryStartTick_ = 0;
  Tick recoveredMs_ = 0;
  int64_t recoveryRemainder_ = 0;
  Tick insightExpiresAt_ = 0;
  bool insightAvailable_ = false;
  std::array<Tick, 3> sourceReadyAt_{};
  std::array<std::optional<Tick>, 3> distinctSourceTicks_{};
  FixedPoint resonance_ = 0;
  Tick ultimateWindowExpiresAt_ = 0;
  bool ultimateWindowActive_ = false;
};
