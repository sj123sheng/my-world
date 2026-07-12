#include "../native/gameplay/combat/event.h"
#include <algorithm>
#include <cassert>
int main(){
  std::vector<HitEvent> v{
    {1,2,0,SourceType::Radiance,fp(1),fp(10), 5, 2},
    {1,2,0,SourceType::Current ,fp(1),fp(10), 5, 1}};
  std::sort(v.begin(), v.end(), [](auto&a,auto&b){
    return a.tick!=b.tick ? a.tick<b.tick : a.sequence<b.sequence; });
  assert(v[0].sequence==1 && v[1].sequence==2);
  return 0;
}
