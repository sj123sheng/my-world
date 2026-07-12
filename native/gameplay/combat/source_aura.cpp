#include "source_aura.h"
void SourceAuraContainer::apply(const SourceAura& a){ auras_.push_back(a); }
void SourceAuraContainer::decay(Tick now){
  for (auto it=auras_.begin(); it!=auras_.end(); )
    if (it->expireAt <= now) it = auras_.erase(it); else ++it;
}
