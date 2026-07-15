#pragma once
#include <native_window/external_window.h>
#include <native_buffer/native_buffer.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <vector>
#include <random>
#include <cstdint>
#include <mutex>
#include "native/gameplay/player/player_controller.h"

struct Particle {
  float x;
  float y;
  float life;
  float maxLife;
};

struct Prop {
  float x;
  float y;
  float size;
  int kind; // 0 = tree, 1 = rock
};

struct Surface {
  std::mutex windowMutex;
  OHNativeWindow* window = nullptr;
  OH_NativeBuffer* nativeBuffer = nullptr;
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLSurface surface = EGL_NO_SURFACE;
  EGLContext context = EGL_NO_CONTEXT;
  EGLConfig config = nullptr;
  GLuint program = 0;
  GLint locPosition = -1;
  GLint locColor = -1;
  int32_t width = 0;
  int32_t height = 0;
  int32_t stride = 0;
  int32_t bufferFormat = 0;
  bool useSoftware = false;
  bool ready = false;
  bool glWindowCreated = false;
  Player player;
  std::vector<Particle> particles;
  std::vector<Prop> props;
  std::vector<uint32_t> pixelBuffer;
};

bool surface_init(Surface& s, OHNativeWindow* window);
bool surface_resize(Surface& s, OHNativeWindow* window);
void surface_draw(Surface& s);
void surface_swap(Surface& s);
void surface_destroy(Surface& s);
