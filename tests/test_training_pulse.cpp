#include "../native/gameplay/combat/training_pulse.h"

#include <cassert>

int main() {
  TrainingPulse pulse(CombatConfig::defaults());
  assert(pulse.advance(0).kind == PulseEventKind::Warning);
  assert(pulse.advance(799).kind == PulseEventKind::None);
  const auto firstHit = pulse.advance(800);
  assert(firstHit.kind == PulseEventKind::Hit && firstHit.tick == 800);
  assert(pulse.classifyDodge(700) == DodgeGrade::Precise);
  assert(pulse.classifyDodge(900) == DodgeGrade::Precise);
  assert(pulse.classifyDodge(599) == DodgeGrade::Normal);
  assert(pulse.classifyDodge(901) == DodgeGrade::Normal);

  const auto nextWarning = pulse.advance(3000);
  assert(nextWarning.kind == PulseEventKind::Warning && nextWarning.tick == 3000);
  const auto nextHit = pulse.advance(3800);
  assert(nextHit.kind == PulseEventKind::Hit && nextHit.tick == 3800);

  TrainingPulse largeStep(CombatConfig::defaults());
  const auto crossed = largeStep.advance(6800);
  assert(crossed.kind == PulseEventKind::Hit && crossed.tick == 6800);
  assert(largeStep.advance(6800).kind == PulseEventKind::None);

  TrainingPulse directToFirstHit(CombatConfig::defaults());
  assert(directToFirstHit.advance(800).kind == PulseEventKind::Hit);
  return 0;
}
