#include "resonance.h"
std::optional<ResonanceType> resolveResonance(SourceType a, SourceType b){
  if (a == b) return std::nullopt;
  if ((a==SourceType::Radiance && b==SourceType::Current) ||
      (a==SourceType::Current && b==SourceType::Radiance)) return ResonanceType::Refraction;
  if ((a==SourceType::Current && b==SourceType::Corruption) ||
      (a==SourceType::Corruption && b==SourceType::Current)) return ResonanceType::Stasis;
  if ((a==SourceType::Corruption && b==SourceType::Radiance) ||
      (a==SourceType::Radiance && b==SourceType::Corruption)) return ResonanceType::Collapse;
  return std::nullopt;
}
