#pragma once
struct Boss { int phase=1; bool transition(int hpPct){ if(hpPct<50 && phase==1){phase=2; return true;} return false; } };
