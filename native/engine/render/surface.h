#pragma once
#ifdef OHOS_PLATFORM
#include <native_window/external_window.h>
#include <native_buffer/native_buffer.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#else
struct OHNativeWindow;
struct OH_NativeBuffer;
using EGLDisplay = void*;
using EGLSurface = void*;
using EGLContext = void*;
using EGLConfig = void*;
using GLuint = unsigned int;
using GLint = int;
inline constexpr EGLDisplay EGL_NO_DISPLAY = nullptr;
inline constexpr EGLSurface EGL_NO_SURFACE = nullptr;
inline constexpr EGLContext EGL_NO_CONTEXT = nullptr;
#endif
#include <vector>
#include <random>
#include <cstdint>
#include <mutex>
#include "native/gameplay/player/player_controller.h"
#include "native/engine/render/camera_render_state.h"

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

struct TrainingTargetRenderState {
  uint32_t id = 1001;
  float x = 0.5f;
  float y = 0.8f;
  float size = 0.045f;
  bool alive = true;
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
  CameraRenderState cameraRenderState;
  std::vector<Particle> particles;
  std::vector<Prop> props;
  TrainingTargetRenderState trainingTarget;
  std::vector<uint32_t> pixelBuffer;
};

bool surface_init(Surface& s, OHNativeWindow* window);
bool surface_resize(Surface& s, OHNativeWindow* window);
void surface_draw(Surface& s);
void surface_swap(Surface& s);
void surface_destroy(Surface& s);
