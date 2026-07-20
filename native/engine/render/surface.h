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

#include "native/engine/render/camera3d.h"
#include "native/engine/render/mesh.h"
#include "native/engine/render/shader_3d.h"
#include <glm/vec3.hpp>

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

// 3D 渲染层使用的实体状态。渲染层只读消费 2D 逻辑写入的位置与存活状态，
// 不反向修改游戏逻辑。archetype/phase 以 int 存储，避免 surface.h 拉入
// gameplay 枚举头文件，保持渲染层与逻辑层头文件依赖单向。
struct Enemy3DRenderState {
  float x = 0.5f;
  float y = 0.5f;
  // 0 = RiftClaw, 1 = Priest, 2 = Guard（与 EnemyArchetype 数值一致）。
  int archetype = 0;
  bool alive = false;
};

struct Boss3DRenderState {
  float x = 0.5f;
  float y = 0.75f;
  // 1 = RadianceLockdown, 2 = CurrentStorm, 3 = CorruptionCollapse
  //（与 BossPhase 数值一致）。
  int phase = 1;
  bool defeated = false;
  bool active = false;
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

  // ---- 3D 渲染层字段（M3-1）----
  // 与 2D 单色着色器独立，仅在 OHOS_PLATFORM 下由 surface_draw 的 3D 阶段消费。
  Camera3D camera3d;
  Mesh playerMesh;
  Mesh groundMesh;
  Mesh enemyMesh;
  Mesh bossMesh;
  Shader3D shader3d;
  glm::vec3 lightDir{0.35f, 0.85f, 0.25f};
  glm::vec3 lightColor{0.8f, 0.8f, 0.75f};
  glm::vec3 ambient{0.25f, 0.25f, 0.3f};
  std::vector<Enemy3DRenderState> enemies3d;
  Boss3DRenderState boss3d;
  bool shader3dReady = false;
};

bool surface_init(Surface& s, OHNativeWindow* window);
bool surface_resize(Surface& s, OHNativeWindow* window);
void surface_draw(Surface& s);
void surface_swap(Surface& s);
void surface_destroy(Surface& s);
