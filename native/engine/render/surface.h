#pragma once
#include <native_window.h>
struct Surface { void* eglDisplay=nullptr; void* eglSurface=nullptr; NativeWindow* window=nullptr; };
bool surface_create(Surface& s, const char* surfaceId);
void surface_present(Surface& s);
