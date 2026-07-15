#include "../native/gameplay/combat/resonance.h"
#include <cassert>
int main(){
  assert(resolveResonance(SourceType::Radiance, SourceType::Current)==ResonanceType::Refraction);
  assert(resolveResonance(SourceType::Current, SourceType::Radiance)==ResonanceType::Refraction);
  assert(resolveResonance(SourceType::Current, SourceType::Corruption)==ResonanceType::Stasis);
  assert(resolveResonance(SourceType::Corruption, SourceType::Current)==ResonanceType::Stasis);
  assert(resolveResonance(SourceType::Corruption, SourceType::Radiance)==ResonanceType::Collapse);
  assert(resolveResonance(SourceType::Radiance, SourceType::Corruption)==ResonanceType::Collapse);
  assert(!resolveResonance(SourceType::Radiance, SourceType::Radiance));
  return 0;
}
