#include "source_aura.h"
#include <algorithm>

void SourceAuraContainer::apply(const SourceAura& a){
  const auto existing = std::find_if(auras_.begin(), auras_.end(), [&](const SourceAura& aura) {
    return aura.type == a.type;
  });
  if (existing != auras_.end()) {
    *existing = a;
  } else {
    auras_.push_back(a);
  }
}
void SourceAuraContainer::decay(Tick now){
  for (auto it=auras_.begin(); it!=auras_.end(); )
    if (it->expireAt <= now) it = auras_.erase(it); else ++it;
}
bool SourceAuraContainer::consume(SourceType type) {
  const auto found = std::find_if(auras_.begin(), auras_.end(), [&](const SourceAura& aura) {
    return aura.type == type;
  });
  if (found == auras_.end()) return false;
  auras_.erase(found);
  return true;
}
