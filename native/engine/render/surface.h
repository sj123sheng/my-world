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
#include "native/engine/render/bloom_pass.h"
#include "native/engine/render/skinned_model.h"
#include "native/engine/render/static_model.h"
#include "native/engine/render/environment.h"
#include "native/engine/world/terrain_heightfield.h"
#include <glm/vec3.hpp>

class StreamScheduler;

struct Particle {
  float x;
  float y;
  float life;
  float maxLife;
};

// 3D 命中火花：伤害命中时在目标位置爆发的短命粒子，
// 由逻辑层做速度积分与寿命衰减，渲染层画广告牌四边形。
// 也复用为攻击/技能“释放过程”飞行投射物：逻辑层给定
// 指向目标的速度，寿命即飞行时长。
struct HitSpark3D {
  float x;
  float y;  // 高度（3D 空间 Y）
  float z;
  float vx;
  float vy;
  float vz;
  float life;
  float maxLife;
  int kind;  // 0=命中金橙 1=玩家受击红 2=击杀亮金 3=移动尾迹
             // 4=辉印金白 5=脉流青蓝 6=蚀质暗紫
             // 7=主角武器拖尾金白 8=敌方武器拖尾红
  // 渲染尺寸倍率：按归属实体的模型缩放比例同步放大特效，
  // 保证模型变大/变小时火花与投射物尺寸始终匹配。
  float sizeScale = 1.0f;
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
  // 0 = RiftClaw, 1 = Priest, 2 = Guard, 3 = Bruiser, 4 = Caster,
  // 5 = Elite（与 EnemyArchetype 数值一致）。
  int archetype = 0;
  bool alive = false;
  // 处于攻击前摇：渲染层据此绘制脚下预警环。
  bool windingUp = false;
  // 攻击已挥出：逻辑层据此做释放动效的上升沿检测。
  bool attacking = false;
  ActorRenderState animation;
  float angle = 0.0f;  // 朝向角，弧度
  // 死亡后的累计秒数：驱动尸体淡出曲线（DeathFadeAlpha）。
  float deathSeconds = 0.0f;
  // 累计受击次数：按奇偶驱动受击/死亡动画变体轮换。
  uint32_t hitCount = 0;
  // 元素附着位掩码：bit0=辉印 bit1=脉流 bit2=蚀质（SourceType 位序）。
  // 渲染层据此在脚下绘制对应元素色的呼吸光环（原神式附着指示）。
  int auraMask = 0;
};

// 野外敌人（WildSpawnSystem）的 3D 渲染状态：字段与 Enemy3DRenderState
// 对齐（渲染层复用同一绘制路径），id 从 5000 起与遭遇敌人无冲突。
struct WildEnemy3DRenderState {
  uint32_t id = 0;
  float x = 0.5f;
  float y = 0.5f;
  // 与 EnemyArchetype 数值一致（0-5）。
  int archetype = 0;
  bool alive = false;
  bool windingUp = false;
  bool attacking = false;
  ActorRenderState animation;
  float angle = 0.0f;
  float deathSeconds = 0.0f;
  uint32_t hitCount = 0;
};

// NPC（Phase 4）的 3D 渲染状态：字段最小化，仅 idle/walk 动画，
// 无血条/受击/锁定逻辑；发布侧已按距玩家距离裁剪到同屏上限。
struct Npc3DRenderState {
  uint32_t id = 0;
  float x = 0.5f;
  float y = 0.5f;
  float angle = 0.0f;
  ActorRenderState animation;
  bool visible = true;
  // 0=Idle 1=Patrol（与 WorldLayout::NpcBehavior 数值一致）。
  int behavior = 0;
};

struct Boss3DRenderState {
  float x = 0.5f;
  float y = 0.75f;
  // 1 = RadianceLockdown, 2 = CurrentStorm, 3 = CorruptionCollapse
  //（与 BossPhase 数值一致）。
  int phase = 1;
  bool defeated = false;
  bool active = false;
  // 首领正在吟唱机制（前摇）：渲染层据此绘制脚下预警环。
  bool windingUp = false;
  // 当前机制（与 BossMechanic 数值一致）：1=JudgmentBeam 时绘制光束轨迹。
  int mechanic = 0;
  ActorRenderState animation;
  float angle = 0.0f;  // 朝向角，弧度
  int64_t previousHp = 0;
  float hitAnimationSeconds = 0.0f;
  float cinematicProgress = 0.0f;
  uint8_t shardCount = 3;
  uint8_t sourceColor = 0;
  bool ringBroken = false;
  // 软锁定当前命中首领：渲染层据此给首领轮廓光常亮增强。
  bool targeted = false;
  // 首领激活后的累计秒数：驱动出场轮廓光渐入（BossEntranceReveal）。
  float entranceSeconds = 0.0f;
  // 首领自由移动中：驱动跑动动画与步频。
  bool moving = false;
  // 首领普攻前摇中：驱动攻击动画与预警环。
  bool basicAttacking = false;
  // 普攻变体（0/1/2）：选择差异化的释放特效配色与齐射规模。
  uint8_t basicAttackVariant = 0;
};

// 敌人头顶血条渲染状态：逻辑侧每帧生成，渲染层绘制为面向相机的
// 背景条 + 按比例缩短的前景条；trailRatio 为延迟追赶的扣血滞后条。
struct EnemyHpBarRenderState {
  float x = 0.0f;
  float z = 0.0f;
  float ratio = 1.0f;  // hp / maxHp，[0, 1]
  float trailRatio = 1.0f;  // 滞后条比例，受击后延迟追赶 ratio
};

// 锁定目标指示器渲染状态：软瞄准命中的目标脚下绘制脉冲环。
struct TargetMarkerRenderState {
  float x = 0.0f;
  float z = 0.0f;
  float pulsePhase = 0.0f;  // 弧度，驱动缩放与旋转脉冲
  bool active = false;
  // 当前锁定目标的 EntityId（0 = 无锁定）：渲染层据此给对应
  // 敌人轮廓光常亮增强，与脚下指示环形成双重锁定反馈。
  uint32_t targetId = 0;
};

// 伤害飘字渲染状态：逻辑侧每帧生成，渲染层绘制为面向相机的
// 数字广告牌（billboard），kind 对应 DamageNumberKind。
struct DamageNumberRenderState {
  float x = 0.0f;
  float z = 0.0f;
  float rise = 0.0f;
  float driftX = 0.0f;
  float alpha = 1.0f;
  float scale = 1.0f;  // 入场弹出缩放（0.6 → 1.0）
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
  // ---- 地形/水面/天空（地图渲染重设计）----
  // terrainMesh 采样与逻辑层同一 TerrainHeightfield，保证视觉与
  // 贴地/坡度/水域判定严格一致；skyMesh 为单位球体，绘制时以相机为
  // 心放大成穹顶；waterMesh 为单位平面，绘制时抬升到水面高度。
  Mesh terrainMesh;
  Mesh waterMesh;
  Mesh skyMesh;
  // 分块地形流式渲染：调度器由 Loop 注入（只读消费），
  // terrainChunkMeshes 为已上传 GPU 的分块网格（chunk id → Mesh），
  // 卸载路径沿用 abandonGpuResources/destroy 模式。
  StreamScheduler* streamScheduler = nullptr;
  std::unordered_map<int32_t, Mesh> terrainChunkMeshes;
  // 逻辑层高度场只读指针：由 Loop 注入，渲染层据此贴地采样。
  const TerrainHeightfield* terrain = nullptr;
  // 主角脚底 3D 高度：逻辑层每帧写入（motionState.height），
  // 渲染层直接消费，保证跳跃/滑翔/游泳时模型与相机同步。
  float playerGroundHeight = 0.0f;
  // 渲染时钟（秒）：驱动水面流动涟漪。
  float renderSeconds = 0.0f;
  Shader3D shader3d;
  glm::vec3 lightDir{0.35f, 0.85f, 0.25f};
  glm::vec3 lightColor{0.8f, 0.8f, 0.75f};
  glm::vec3 ambient{0.25f, 0.25f, 0.3f};
  std::vector<Enemy3DRenderState> enemies3d;
  // 野外敌人渲染列表（Phase 3.2/3.3），与 enemies3d 共用绘制/状态表。
  std::vector<WildEnemy3DRenderState> wildEnemies3d;
  // NPC 渲染列表（Phase 4）：同屏 ≤6，超出按距玩家距离裁剪。
  std::vector<Npc3DRenderState> npcs3d;
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
  // NPC 模型槽位（Phase 4）：第一版由应用层注入 player.glb 占位字节，
  // 缺失时保持静态 Mesh 回退，不影响其余槽位。
  PendingModelAsset npcModelAsset;
  std::array<PendingModelAsset, 4> environmentAssets;
  std::array<StaticModel, 4> environmentModels;
  std::array<EnvironmentBatchStatus, 4> environmentStatuses{
      EnvironmentBatchStatus::Empty, EnvironmentBatchStatus::Empty,
      EnvironmentBatchStatus::Empty, EnvironmentBatchStatus::Empty};
  // Phase 2 区块环境批次（blockId → 待上传字节/模型/状态）：
  // block_<id>.glb 由应用层按玩家所在分块懒注入，仅在对应分块
  // 激活时绘制；缺失的区块 GLB 保持 Empty，不影响全局批次。
  std::unordered_map<int32_t, PendingModelAsset> blockEnvironmentAssets;
  std::unordered_map<int32_t, StaticModel> blockEnvironmentModels;
  std::unordered_map<int32_t, EnvironmentBatchStatus> blockEnvironmentStatuses;
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
  SkinnedModel npcModel;
  SkinnedAnimationState playerAnimationState;
  SkinnedAnimationState trainingTargetAnimationState;
  SkinnedAnimationState bossAnimationState;
  std::unordered_map<uint32_t, SkinnedAnimationState> enemyAnimationStates;
  // NPC 逐实体动画播放状态（共享 npcModel 网格/纹理，仅复制播放状态）。
  std::unordered_map<uint32_t, SkinnedAnimationState> npcAnimationStates;
  // 受击闪白计时器：实体 id → 剩余秒数。由逻辑层从 Damage 事件写入，
  // 渲染层据此把模型配色向白色提亮，给出“打中了”的即时反馈。
  std::unordered_map<uint32_t, float> enemyHitFlash;
  // 死亡淡出计时器：实体 id → 死亡后累计秒数。逻辑层逐帧推进，
  // 渲染层据此把尸体模型线性淡出到完全移除。
  std::unordered_map<uint32_t, float> enemyDeathSeconds;
  // 受击计数器：实体 id → 累计受击次数，驱动受击/死亡动画变体轮换。
  std::unordered_map<uint32_t, uint32_t> enemyHitCounts;
  // 敌方释放动效边沿状态：前摇开始触发蓄力火花，挥击瞬间
  // 触发朝主角的投射物；与主角侧释放特效对称。
  std::unordered_map<uint32_t, bool> enemyPrevWindingUp;
  std::unordered_map<uint32_t, bool> enemyPrevAttacking;
  bool bossPrevWindingUp = false;
  // 首领普攻边沿状态：前摇上升沿触发蓄力爆发，挥击下降沿触发齐射。
  bool bossPrevBasicAttacking = false;
  bool shader3dReady = false;
  // ---- bloom 后处理资源（原神式技能发光）----
  // 逻辑侧按画质档位写入（高画质 true）；渲染侧据此决定场景是否
  // 先入 FBO 再做亮通提取/模糊/合成。资源随窗口尺寸惰性重建。
  bool bloomEnabled = true;
  bool bloomReady = false;
  GLuint bloomProgram = 0;    // 亮通/模糊/合成三合一程序（uMode 切换）
  GLuint bloomSceneFbo = 0;   // 场景颜色 FBO（全分辨率）
  GLuint bloomSceneTex = 0;   // 场景颜色纹理 RGBA8
  GLuint bloomDepthRbo = 0;   // 场景深度 renderbuffer
  GLuint bloomPingFbo = 0;    // 半分辨率 ping-pong FBO A
  GLuint bloomPongFbo = 0;    // 半分辨率 ping-pong FBO B
  GLuint bloomPingTex = 0;    // 半分辨率纹理 A
  GLuint bloomPongTex = 0;    // 半分辨率纹理 B
  int bloomFboWidth = 0;      // 当前 FBO 对应的窗口尺寸（重建判据）
  int bloomFboHeight = 0;
  AssetProfile playerAssetProfile = AssetProfile::forModel(ModelKind::Player);
  AssetProfile enemyAssetProfile = AssetProfile::forModel(ModelKind::Enemy);
  AssetProfile bossAssetProfile = AssetProfile::forModel(ModelKind::Boss);
  AssetProfile npcAssetProfile = AssetProfile::forModel(ModelKind::Npc);
  int32_t vfxFlags = 0;
  float vfxHitFlash = 0.0f;
  float vfxDodgeFlash = 0.0f;
  float vfxResonanceBurst = 0.0f;
  float vfxCameraShakeX = 0.0f;
  float vfxCameraShakeY = 0.0f;
  // 预警环脉冲时钟（秒），由逻辑层逐帧累加，供渲染层做呼吸动画。
  float windupPulseSeconds = 0.0f;
  // 元素附着光环呼吸时钟（秒）：按 AuraRingPeriod() 回绕，
  // 驱动附着光环的半径/透明度脉动。
  float auraPulseSeconds = 0.0f;
  // 附着粒子发射累加器（秒）：超过 AuraParticleInterval() 时为
  // 每个附着源质发射一颗上升粒子并回绕。
  float auraEmitSeconds = 0.0f;
  // 训练假人的元素附着掩码（bit0=辉印 bit1=脉流 bit2=蚀质）：
  // 训练模式下取自战斗快照附着位，其余模式恒 0。
  int trainingTargetAuraMask = 0;

  // ---- 命中火花字段 ----
  std::vector<HitSpark3D> hitSparks3d;
  // 火花发射伪随机种子（LCG），保证同输入下方向可重现。
  uint32_t hitSparkSeed = 0;

  // ---- 普攻刀光与技能冲击波字段 ----
  // 新月形刀光单位网格（XZ 平面，外径 1.0），绘制时按角色缩放定位。
  Mesh slashArcMesh;
  // 主角佩剑网格（柄底原点、刃沿 +Y）：按 handslot.r 关节矩阵挂载，
  // 随攻击/跑动动画挥舞；playerWeaponJoint<0 时不挂载。
  Mesh swordMesh;
  int playerWeaponJoint = -1;
  // 敌方法杖与首领重棍：同一 handslot.r 挂点机制，按角色档案缩放。
  Mesh staffMesh;
  Mesh clubMesh;
  int enemyWeaponJoint = -1;
  int bossWeaponJoint = -1;
  // 主角刀光状态：seconds<0 表示无激活刀光；挥击边沿由 Loop 写入
  // 起点秒数 0 与当时朝向，渲染层按 SlashArcPoseAt 扫掠绘制。
  float playerSlashSeconds = -1.0f;
  int playerSlashCombo = 0;
  float playerSlashYaw = 0.0f;
  // 敌方普攻刀光：遭遇敌人挥击边沿触发，按实体存活与时长裁剪。
  struct EnemySlashArc {
    uint32_t id = 0;
    float x = 0.0f;
    float y = 0.0f;
    float yaw = 0.0f;
    float seconds = 0.0f;
    float scale = 1.0f;
  };
  std::vector<EnemySlashArc> enemySlashArcs;
  // 技能释放冲击波环：施法边沿在施法者脚下生成，扩张并淡出。
  struct ShockwaveRing {
    float x = 0.0f;
    float z = 0.0f;
    float seconds = 0.0f;
    float maxRadius = 0.0f;
    glm::vec3 color{1.0f};
  };
  std::vector<ShockwaveRing> shockwaveRings;
  // 命中贴地冲击贴花：伤害命中点在受击实体脚下浮现的短促光斑。
  struct ImpactDecal {
    float x = 0.0f;
    float z = 0.0f;
    float seconds = 0.0f;
    float maxRadius = 0.0f;
    glm::vec3 color{1.0f};
  };
  std::vector<ImpactDecal> impactDecals;
  // 共鸣爆发光柱：元素反应触发瞬间从受击点升起的垂直光柱。
  struct LightPillar {
    float x = 0.0f;
    float z = 0.0f;
    float seconds = 0.0f;
    float maxHeight = 0.0f;
    glm::vec3 color{1.0f};
  };
  std::vector<LightPillar> lightPillars;

  // 玩家当前血量比例（0..1）：低于阈值时渲染层绘制边缘脉冲警示。
  float playerHpRatio = 1.0f;
  // 玩家处于无敌帧（闪避中）：渲染层半透明化给出清晰的免伤反馈。
  bool playerInvulnerable = false;

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
      case ModelKind::Npc:
        npcAssetProfile = profile;
        break;
    }
  }

  void setEnvironmentPalette(const EnvironmentPalette& palette) {
    environmentPalette = palette;
  }

  void pruneEnemyAnimationStates() {
    for (auto state = enemyAnimationStates.begin();
         state != enemyAnimationStates.end();) {
      const bool present =
          std::any_of(enemies3d.begin(), enemies3d.end(),
                      [id = state->first](const Enemy3DRenderState& enemy) {
                        return enemy.id == id;
                      }) ||
          std::any_of(wildEnemies3d.begin(), wildEnemies3d.end(),
                      [id = state->first](const WildEnemy3DRenderState& enemy) {
                        return enemy.id == id;
                      });
      if (present) {
        ++state;
      } else {
        state = enemyAnimationStates.erase(state);
      }
    }
  }

  void pruneNpcAnimationStates() {
    for (auto state = npcAnimationStates.begin();
         state != npcAnimationStates.end();) {
      const bool present = std::any_of(
          npcs3d.begin(), npcs3d.end(),
          [id = state->first](const Npc3DRenderState& npc) {
            return npc.id == id;
          });
      if (present) {
        ++state;
      } else {
        state = npcAnimationStates.erase(state);
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
      case ModelKind::Npc:
        npcModelAsset.replace(std::move(bytes));
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

  // 注入区块批次字节（blockId ∈ [0, kEnvironmentBlockCount)）；
  // 解析与上传由 current GL context 下的渲染路径完成。
  void setBlockEnvironmentAsset(int32_t blockId, std::vector<uint8_t> bytes) {
    if (blockId < 0 || blockId >= kEnvironmentBlockCount) return;
    std::lock_guard<std::mutex> lock(modelAssetMutex);
    blockEnvironmentAssets[blockId].replace(std::move(bytes));
    blockEnvironmentStatuses[blockId] = EnvironmentBatchStatus::Pending;
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
