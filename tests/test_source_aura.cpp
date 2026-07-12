#include "../native/gameplay/combat/source_aura.h"
#include <cassert>
int main(){
  SourceAuraContainer c;
  c.apply({SourceType::Radiance, fp(1.0), 100, 1});
  c.decay(50);  assert(c.active().size()==1);
  c.decay(100); assert(c.active().size()==0);
  return 0;
}
