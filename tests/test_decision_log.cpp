#include "../native/gameplay/combat/decision_log.h"
#include <cassert>
int main(){
  DecisionLog a,b;
  HitEvent h{1,2,0,SourceType::Radiance,fp(1),fp(10),5,1};
  CombatResult r{fp(10),fp(2),ResonanceType::Refraction,{},{}};
  a.record(h,r); b.record(h,r);
  assert(a.replayEquals(b));
  return 0;
}
