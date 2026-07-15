#include "../native/gameplay/combat/training_pulse.h"

#include <cassert>

int main() {
  TrainingPulse pulse(CombatConfig::defaults());
  const auto initial = pulse.advance(0);
  assert(initial.size() == 1);
  assert(initial[0].kind == PulseEventKind::Warning && initial[0].tick == 0);
  assert(pulse.advance(799).empty());
  const auto firstHit = pulse.advance(800);
  assert(firstHit.size() == 1);
  assert(firstHit[0].kind == PulseEventKind::Hit && firstHit[0].tick == 800);
  assert(pulse.classifyDodge(700) == DodgeGrade::Precise);
  assert(pulse.classifyDodge(900) == DodgeGrade::Precise);
  assert(pulse.classifyDodge(599) == DodgeGrade::Normal);
  assert(pulse.classifyDodge(901) == DodgeGrade::Normal);

  const auto nextWarning = pulse.advance(3000);
  assert(nextWarning.size() == 1);
  assert(nextWarning[0].kind == PulseEventKind::Warning && nextWarning[0].tick == 3000);
  const auto nextHit = pulse.advance(3800);
  assert(nextHit.size() == 1);
  assert(nextHit[0].kind == PulseEventKind::Hit && nextHit[0].tick == 3800);

  TrainingPulse largeStep(CombatConfig::defaults());
  const auto crossed = largeStep.advance(6800);
  assert(crossed.size() == 6);
  const Tick expectedTicks[] = {0, 800, 3000, 3800, 6000, 6800};
  const PulseEventKind expectedKinds[] = {
      PulseEventKind::Warning, PulseEventKind::Hit, PulseEventKind::Warning,
      PulseEventKind::Hit, PulseEventKind::Warning, PulseEventKind::Hit};
  for (std::size_t index = 0; index < crossed.size(); ++index) {
    assert(crossed[index].tick == expectedTicks[index]);
    assert(crossed[index].kind == expectedKinds[index]);
  }
  assert(largeStep.advance(6800).empty());
  assert(largeStep.advance(6799).empty());

  TrainingPulse directToFirstHit(CombatConfig::defaults());
  const auto direct = directToFirstHit.advance(800);
  assert(direct.size() == 2);
  assert(direct[0].kind == PulseEventKind::Warning && direct[0].tick == 0);
  assert(direct[1].kind == PulseEventKind::Hit && direct[1].tick == 800);
  return 0;
}
