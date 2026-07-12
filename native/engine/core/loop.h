#pragma once
#include "../render/surface.h"
struct Loop { Surface surface; bool running=false; void start(const char* id); void stop(); void tickOnce(); };
