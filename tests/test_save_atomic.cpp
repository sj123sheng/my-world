#include "../native/engine/resource/save.h"
#include <cassert>
int main(){
  Save s; SaveState st{2,3,5};
  assert(s.write(st,"/tmp/save.dat"));
  SaveState out{0,0,0};
  assert(s.read(out,"/tmp/save.dat"));
  assert(out.campLevel==2 && out.relics==3 && out.regionProgress==5);
  return 0;
}
