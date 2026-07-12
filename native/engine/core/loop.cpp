#include "loop.h"
void Loop::start(const char* id){ running=true; surface_create(surface, id); }
void Loop::stop(){ running=false; }
void Loop::tickOnce(){
  // M1 在此推进固定 tick 战斗;M0 仅清屏 + 统计
  surface_present(surface);
}
