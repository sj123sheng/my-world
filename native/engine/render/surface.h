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
#include <algorithm>
#include <random>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <array>
#include "native/gameplay/player/player_controller.h"
#include "native/engine/render/camera_render_state.h"
#include "native/engine/render/asset_profile.h"
#include "native/engine/presentation/visual_tokens.h"

#include "native/engine/render/camera3d.h"
#include "native/engine/render/mesh.h"
#include "native/engine/render/render_animation.h"
#include "native/engine/render/render_lifecycle.h"
#include "native/engine/render/shader_3d.h"
#include "native/engine/render/skinned_model.h"
#include "native/engine/render/static_model.h"
#include "native/engine/render/environment.h"
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
  // gameplay EntityId 的稳定值；使用底层类型避免渲染层反向依赖战斗头文件。
  uint32_t id = 0;
  float x = 0.5f;
  float y = 0.5f;
  // 0 = RiftClaw, 1 = Priest, 2 = Guard（与 EnemyArchetype 数值一致）。
  int archetype = 0;
  bool alive = false;
  ActorRenderState animation;
  float angle = 0.0f;  // 朝向角，弧度
};

struct Boss3DRenderState {
  float x = 0.5f;
  float y = 0.75f;
  // 1 = RadianceLockdown, 2 = CurrentStorm, 3 = CorruptionCollapse
  //（与 BossPhase 数值一致）。
  int phase = 1;
  bool defeated = false;
  bool active = false;
  ActorRenderState animation;
  float angle = 0.0f;  // 朝向角，弧度
  int64_t previousHp = 0;
  float hitAnimationSeconds = 0.0f;
  float cinematicProgress = 0.0f;
  uint8_t shardCount = 3;
  uint8_t sourceColor = 0;
  bool ringBroken = false;
};

// 敌人头顶血条渲染状态：逻辑侧每帧生成，渲染层绘制为面向相机的
// 背景条 + 按比例缩短的前景条。
struct EnemyHpBarRenderState {
  float x = 0.0f;
  float z = 0.0f;
  float ratio = 1.0f;  // hp / maxHp，[0, 1]
};

// 锁定目标指示器渲染状态：软瞄准命中的目标脚下绘制脉冲环。
struct TargetMarkerRenderState {
  float x = 0.0f;
  float z = 0.0f;
  float pulsePhase = 0.0f;  // 弧度，驱动缩放与旋转脉冲
  bool active = false;
};

// 伤害飘字渲染状态：逻辑侧每帧生成，渲染层绘制为面向相机的
// 数字广告牌（billboard），kind 对应 DamageNumberKind。
struct DamageNumberRenderState {
  float x = 0.0f;
  float z = 0.0f;
  float rise = 0.0f;
  float driftX = 0.0f;
  float alpha = 1.0f;
  int32_t value = 0;
  int kind = 0;
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
  Mesh bossRingMesh;
  Shader3D shader3d;
  glm::vec3 lightDir{0.35f, 0.85f, 0.25f};
  glm::vec3 lightColor{0.8f, 0.8f, 0.75f};
  glm::vec3 ambient{0.25f, 0.25f, 0.3f};
  std::vector<Enemy3DRenderState> enemies3d;
  Boss3DRenderState boss3d;
  ActorRenderState player3dAnimation;
  ActorRenderState trainingTarget3dAnimation;
  float playerHitAnimationSeconds = 0.0f;

  // ---- 伤害飘字字段 ----
  std::vector<DamageNumberRenderState> damageNumbers3d;
  Mesh digitMeshes[10];  // 每个数字一个单位四边形，UV 预烘焙到图集单元
  unsigned int digitAtlasTexture = 0;
  bool digitAssetsReady = false;

  // ---- 锁定目标指示器字段 ----
  TargetMarkerRenderState targetMarker3d;
  Mesh targetRingMesh;

  // ---- 接地接触阴影字段 ----
  // 单位圆盘（半径 0.5，法线 +Y），由 model 矩阵缩放到角色脚下尺寸。
  Mesh shadowMesh;

  // ---- 敌人血条字段 ----
  std::vector<EnemyHpBarRenderState> enemyHpBars3d;
  Mesh hpBarQuadMesh;  // 单位四边形（XY 平面，法线 +Z）

  // 三类模型的 bridge 字节可早于或晚于 Surface 创建。setModelAsset 只保存 CPU
  // 数据并标脏；解析、上传、替换和销毁均由 current GL context 下的渲染路径完成。
  std::mutex modelAssetMutex;
  PendingModelAsset playerModelAsset;
  PendingModelAsset enemyModelAsset;
  PendingModelAsset bossModelAsset;
  std::array<PendingModelAsset, 4> environmentAssets;
  std::array<StaticModel, 4> environmentModels;
  std::array<EnvironmentBatchStatus, 4> environmentStatuses{
      EnvironmentBatchStatus::Empty, EnvironmentBatchStatus::Empty,
      EnvironmentBatchStatus::Empty, EnvironmentBatchStatus::Empty};
  EnvironmentController environmentController;
  EnvironmentComposition environmentComposition =
      EnvironmentController::defaultComposition();
  EnvironmentPalette environmentPalette = VisualTokens::environmentPalette();
  EnvironmentRenderPlan environmentPlan;
  Mesh fallbackPillarMesh;
  Mesh fallbackWallMesh;
  Mesh riftPlaneMesh;
  bool environmentReady = false;
  uint32_t environmentDrawCalls = 0;
  uint32_t environmentTriangles = 0;
  int32_t environmentPerfLevel = 0;
  StaticTextureTier loggedEnvironmentTextureTier = StaticTextureTier::Full;
  SkinnedModel playerModel;
  SkinnedModel enemyModel;
  SkinnedModel bossModel;
  SkinnedAnimationState playerAnimationState;
  SkinnedAnimationState trainingTargetAnimationState;
  SkinnedAnimationState bossAnimationState;
  std::unordered_map<uint32_t, SkinnedAnimationState> enemyAnimationStates;
  bool shader3dReady = false;
  AssetProfile playerAssetProfile = AssetProfile::forModel(ModelKind::Player);
  AssetProfile enemyAssetProfile = AssetProfile::forModel(ModelKind::Enemy);
  AssetProfile bossAssetProfile = AssetProfile::forModel(ModelKind::Boss);
  int32_t vfxFlags = 0;
  float vfxHitFlash = 0.0f;
  float vfxDodgeFlash = 0.0f;
  float vfxResonanceBurst = 0.0f;
  float vfxCameraShakeX = 0.0f;
  float vfxCameraShakeY = 0.0f;

  void applyAssetProfile(ModelKind kind, const AssetProfile& profile) {
    switch (kind) {
      case ModelKind::Player:
        playerAssetProfile = profile;
        break;
      case ModelKind::Enemy:
        enemyAssetProfile = profile;
        break;
      case ModelKind::Boss:
        bossAssetProfile = profile;
        break;
    }
  }

  void setEnvironmentPalette(const EnvironmentPalette& palette) {
    environmentPalette = palette;
  }

  void pruneEnemyAnimationStates() {
    for (auto state = enemyAnimationStates.begin();
         state != enemyAnimationStates.end();) {
      const bool present = std::any_of(
          enemies3d.begin(), enemies3d.end(),
          [id = state->first](const Enemy3DRenderState& enemy) {
            return enemy.id == id;
          });
      if (present) {
        ++state;
      } else {
        state = enemyAnimationStates.erase(state);
      }
    }
  }

  void setModelAsset(ModelKind kind, std::vector<uint8_t> bytes) {
    std::lock_guard<std::mutex> lock(modelAssetMutex);
    switch (kind) {
      case ModelKind::Player:
        playerModelAsset.replace(std::move(bytes));
        break;
      case ModelKind::Enemy:
        enemyModelAsset.replace(std::move(bytes));
        break;
      case ModelKind::Boss:
        bossModelAsset.replace(std::move(bytes));
        break;
    }
  }

  void setEnvironmentAsset(EnvironmentBatchKind kind, std::vector<uint8_t> bytes) {
    std::lock_guard<std::mutex> lock(modelAssetMutex);
    const size_t slot = static_cast<size_t>(kind);
    if (slot < environmentAssets.size()) {
      environmentAssets[slot].replace(std::move(bytes));
      environmentStatuses[slot] = EnvironmentBatchStatus::Pending;
    }
  }

  bool shouldDrawEnvironmentFallback() const {
    return environmentStatuses[0] != EnvironmentBatchStatus::Ready ||
           environmentStatuses[1] != EnvironmentBatchStatus::Ready;
  }
};

bool surface_init(Surface& s, OHNativeWindow* window);
bool surface_resize(Surface& s, OHNativeWindow* window);
void surface_draw(Surface& s);
void surface_swap(Surface& s);
void surface_destroy(Surface& s);
