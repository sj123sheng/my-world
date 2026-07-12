#include "resonance.h"
ResonanceType resolveResonance(SourceType a, SourceType b){
  if (a==SourceType::Radiance && b==SourceType::Current)  return ResonanceType::Refraction;
  if (a==SourceType::Current  && b==SourceType::Corruption) return ResonanceType::Stasis;
  if (a==SourceType::Corruption&& b==SourceType::Radiance)  return ResonanceType::Collapse;
  // 三种顺序命中触发 Burst 在 M1-3 处理
  return ResonanceType::Refraction; // 默认占位
}
