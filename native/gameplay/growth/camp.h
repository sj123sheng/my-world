#pragma once
struct Camp { int level=0; bool upgrade(int sourceTraces){ if(sourceTraces<10) return false; level++; return true; } };
