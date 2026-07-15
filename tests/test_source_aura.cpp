#include "../native/gameplay/combat/source_aura.h"
#include <cassert>
int main(){
  SourceAuraContainer c;
  c.apply({SourceType::Radiance, fp(1.0), 100, 1});
  c.decay(50);  assert(c.active().size()==1);
  c.decay(100); assert(c.active().size()==0);
  c.apply({SourceType::Radiance, fp(1.0), 200, 1});
  c.apply({SourceType::Radiance, fp(2.0), 300, 2});
  assert(c.active().size()==1);
  assert(c.active()[0].amount==fp(2.0));
  assert(c.consume(SourceType::Radiance));
  assert(c.active().empty());
  assert(!c.consume(SourceType::Radiance));
  c.apply({SourceType::Current, fp(1.0), 400, 3});
  c.clear();
  assert(c.active().empty());
  return 0;
}
