// render_animation.h: gameplay 快照到渲染动画意图的纯数据映射。

#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

enum class RenderAnimation {
  Idle,
  Run,
  Attack,
  Dodge,
  Radiance,
  Current,
  Corruption,
  Ultimate,
  Hit,
  Death,
  Jump,
  Land,
  // 探索运动语言（主角重制模型新增 clip 驱动）：攀爬/滑翔/转身；
  // 旧资产缺失对应 clip 时 ResolveClip 按回退链退化到既有语言。
  Climb,
  Glide,
  Turn,
};

enum class ModelKind {
  Player,
  Enemy,
  Boss,
  // NPC（Phase 4）：第一版复用 player.glb 占位，独立槽位供后续替换。
  Npc,
};

// 敌人原型数量（0=RiftClaw 1=Priest 2=Guard 3=Bruiser 4=Caster
// 5=Elite）：攻击 clip、武器种类、装备覆盖与独立高模槽位均按该
// 数量对齐。
constexpr int kEnemyArchetypeCount = 6;

struct ActorRenderState {
  bool alive = true;
  RenderAnimation action = RenderAnimation::Idle;
  bool hit = false;
  bool moving = false;
  // 归一化移动输入幅度（0..1）：驱动跑动步频缩放，与地面移速匹配。
  float moveRatio = 1.0f;
  // 受击/死亡动画变体索引：按奇偶在 hit/Hit_B、death/Death_B 之间
  // 轮换，打破连续受击与群体死亡的重复感。
  uint8_t variant = 0;
  // 攻击 clip 偏好（发布侧按连段段数/敌人原型/首领变体写入）：
  // 非空时 ResolveClip 优先选用，缺失自动回退通用 attack。
  std::string attackClip;
};

struct AnimationLogState {
  bool shouldReport(RenderAnimation animation, const std::string& clip) {
    if (initialized && animation == previousAnimation && clip == previousClip) {
      return false;
    }
    initialized = true;
    previousAnimation = animation;
    previousClip = clip;
    return true;
  }

  void reset() {
    initialized = false;
    previousAnimation = RenderAnimation::Idle;
    previousClip.clear();
  }

 private:
  bool initialized = false;
  RenderAnimation previousAnimation = RenderAnimation::Idle;
  std::string previousClip;
};

inline RenderAnimation ChooseAnimation(const ActorRenderState& actor) {
  if (!actor.alive) return RenderAnimation::Death;
  if (actor.action != RenderAnimation::Idle) return actor.action;
  if (actor.hit) return RenderAnimation::Hit;
  if (actor.moving) return RenderAnimation::Run;
  return RenderAnimation::Idle;
}

// 动作转场交叉混合时长（秒）：移动互切 0.15s，进入动作 0.12s，
// 动作恢复到移动/待机 0.2s，死亡转场 0.25s。
inline float AnimationBlendSeconds(RenderAnimation previous,
                                   RenderAnimation requested) {
  if (previous == requested) return 0.0f;
  const bool previousLocomotion =
      previous == RenderAnimation::Idle || previous == RenderAnimation::Run;
  const bool requestedLocomotion =
      requested == RenderAnimation::Idle || requested == RenderAnimation::Run;
  if (requestedLocomotion && previousLocomotion) return 0.15f;
  if (requested == RenderAnimation::Death) return 0.25f;
  if (requestedLocomotion) return 0.2f;
  return 0.12f;
}

// 跑动动画播放速率：按移动输入幅度缩放步频，使脚步节奏与
// 地面移速匹配，消除半推摇杆时的“滑步”；下限保持步态稳定。
inline float RunPlaybackRate(float moveRatio) {
  const float clamped = std::clamp(moveRatio, 0.0f, 1.0f);
  return 0.45f + 0.55f * clamped;
}

// 低速移动步态分层：输入幅度低于阈值时切换行走 clip，而不是
// 只把跑步动画放慢，避免低速时的“慢动作跑”廉价感。
inline bool ShouldUseWalkClip(float moveRatio) {
  constexpr float kWalkThreshold = 0.35f;
  return moveRatio < kWalkThreshold;
}

// clip 播放模式分类：待机/跑动与持续吟唱（Spellcasting）循环播放；
// 主角重制模型的 walk/glide/cast/Jump_Idle 同为循环语言（滑翔
// 空中姿态、施法吟唱、行走步态与空中跳跃姿态均需无缝持续）。
// 攻击、受击、死亡、闪避、单次施法等一次性 clip 播完后必须钳制
// 在尾帧，避免尸体倒地动作或攻击挥砍循环重播。
inline bool IsLoopingClip(const std::string& name) {
  return name == "idle" || name == "run" || name == "Spellcasting" ||
         name == "walk" || name == "glide" || name == "cast" ||
         name == "Jump_Idle";
}

// 死亡尸体淡出曲线：先保持死亡尾帧 0.35s 让玩家看清倒地结果，
// 再用 0.55s 线性淡出到 0 完全移除，避免尸体永久留在场上。
inline float DeathFadeAlpha(float deathSeconds) {
  constexpr float kHoldSeconds = 0.35f;
  constexpr float kFadeSeconds = 0.55f;
  if (deathSeconds <= kHoldSeconds) return 1.0f;
  return std::clamp(1.0f - (deathSeconds - kHoldSeconds) / kFadeSeconds,
                    0.0f, 1.0f);
}

// Boss 出场显现曲线：激活后 0.8s 内从 0 线性升到 1，驱动轮廓光
// 渐入，让 Boss 从黑暗中逐步被勾出，强化登场仪式感。
inline float BossEntranceReveal(float entranceSeconds) {
  constexpr float kRevealSeconds = 0.8f;
  return std::clamp(entranceSeconds / kRevealSeconds, 0.0f, 1.0f);
}

inline const char* RenderAnimationName(RenderAnimation animation) {
  switch (animation) {
    case RenderAnimation::Run:
      return "run";
    case RenderAnimation::Attack:
      return "attack";
    case RenderAnimation::Dodge:
      return "Dodge_Forward";
    case RenderAnimation::Radiance:
      return "Spellcast_Raise";
    case RenderAnimation::Current:
      return "Spellcast_Shoot";
    case RenderAnimation::Corruption:
      return "Spellcasting";
    case RenderAnimation::Ultimate:
      return "Spellcast_Long";
    case RenderAnimation::Hit:
      return "hit";
    case RenderAnimation::Death:
      return "death";
    case RenderAnimation::Jump:
      return "Jump_Idle";
    case RenderAnimation::Land:
      return "Jump_Land";
    case RenderAnimation::Climb:
      return "climb";
    case RenderAnimation::Glide:
      return "glide";
    case RenderAnimation::Turn:
      return "Turn_180";
    case RenderAnimation::Idle:
    default:
      return "idle";
  }
}

// 主角连段攻击 clip（原神四段连招差异化）：1=斜劈 2=横斩 3=突刺、
// 4=双手重劈（终结段，与放大刀光/地面冲击波的分量呼应）；
// 未知段数回退通用 attack。
// 主角方向闪避 clip（原神方向闪避语言）：按移动方向相对角色
// 朝向的带符号夹角（弧度，正 = 左）选前/侧/后闪避姿态——
// |angle| <= pi/4 前闪避、(pi/4, 3pi/4) 侧闪避、>= 3pi/4 后闪避；
// 资产缺失时 ResolveClip 自动回退 Dodge_Forward→run。
inline const char* PlayerDodgeClipFor(float relativeAngleRadians) {
  constexpr float kDiagonal = 0.7853981f;   // pi/4
  constexpr float kBackward = 2.3561945f;   // 3pi/4
  if (relativeAngleRadians > kBackward ||
      relativeAngleRadians < -kBackward) {
    return "Dodge_Backward";
  }
  if (relativeAngleRadians > kDiagonal) return "Dodge_Left";
  if (relativeAngleRadians < -kDiagonal) return "Dodge_Right";
  return "Dodge_Forward";
}

inline const char* PlayerAttackClipFor(int comboSegment) {
  switch (comboSegment) {
    case 1:
      return "1H_Melee_Attack_Slice_Diagonal";
    case 2:
      return "1H_Melee_Attack_Slice_Horizontal";
    case 3:
      return "1H_Melee_Attack_Stab";
    case 4:
      return "2H_Melee_Attack_Chop";
    default:
      return "attack";
  }
}

// 主角跳跃 clip 选取（KayKit 跳跃语言）：离地前 0.18s 播放
// Jump_Start（蹬地起身动量），之后空中 Jump_Idle；滑翔无专属
// clip，复用空中姿态（优于空中播放跑动）。
inline const char* PlayerJumpClipFor(float airSeconds) {
  return airSeconds < 0.18f ? "Jump_Start" : "Jump_Idle";
}

// 敌人原型攻击 clip（原型动作语言差异化）：0=RiftClaw 徒手爪击、
// 1=Priest 仪式施法、2=Guard 盾击、3=Bruiser 双手重斩、
// 4=Caster 法术射击、5=Elite 旋转斩；未知原型回退通用 attack。
inline const char* EnemyAttackClipFor(int archetype) {
  switch (archetype) {
    case 0:
      return "Unarmed_Melee_Attack_Punch_A";
    case 1:
      return "Spellcast_Raise";
    case 2:
      return "Block_Attack";
    case 3:
      return "2H_Melee_Attack_Chop";
    case 4:
      return "Spellcast_Shoot";
    case 5:
      return "2H_Melee_Attack_Spin";
    default:
      return "attack";
  }
}

// 敌人原型武器种类（武器与攻击 clip 语言对齐）：0=无武器
//（RiftClaw 徒手爪击与徒手 clip 一致）1=法杖（Priest/Guard/Caster
// 施法语言）2=长剑（Elite 旋转斩）3=重棍（Bruiser 双手重劈，与
// 首领重棍同分量语言）；未知原型回退法杖。
inline int EnemyWeaponKindFor(int archetype) {
  switch (archetype) {
    case 0:
      return 0;
    case 3:
      return 3;
    case 5:
      return 2;
    default:
      return 1;
  }
}

// 首领普攻变体 clip：0=重劈（金橙挥击）1=吟唱束流（暗紫）
// 2=旋转冲击（青蓝）；与普攻三变体的配色/规模语言对应。
inline const char* BossAttackClipFor(int basicAttackVariant) {
  switch (basicAttackVariant % 3) {
    case 0:
      return "2H_Melee_Attack_Chop";
    case 1:
      return "Spellcast_Long";
    case 2:
      return "2H_Melee_Attack_Spin";
    default:
      return "attack";
  }
}

inline std::string ResolveClip(const std::vector<std::string>& clips,
                               RenderAnimation animation, int variant = 0,
                               float moveRatio = 1.0f,
                               const std::string& preferredAttackClip = {}) {
  std::vector<std::string> candidates{RenderAnimationName(animation)};
  // 攻击 clip 差异化：发布侧写入的段数/原型/变体 clip 优先，
  // 资产缺失时自动回退通用 attack（候选链后段）；跳跃同机制偏好
  // 起跳/空中 clip（Jump_Start/Jump_Idle）。
  if ((animation == RenderAnimation::Attack ||
       animation == RenderAnimation::Jump ||
       animation == RenderAnimation::Dodge) &&
      !preferredAttackClip.empty()) {
    candidates.insert(candidates.begin(), preferredAttackClip);
  }
  // 低速步态分层：低幅度输入优先行走 clip（Walking_B），缺失时
  // 回退主角重制模型的 walk，再缺失时回退 run；资产无行走 clip
  // 时行为与升级前完全一致。
  if (animation == RenderAnimation::Run && ShouldUseWalkClip(moveRatio)) {
    candidates.insert(candidates.begin(), "walk");
    candidates.insert(candidates.begin(), "Walking_B");
  }
  // 受击/死亡变体轮换：奇数变体优先选用 B 版 clip，缺失时自动
  // 回退主 clip，资产无变体时行为与升级前完全一致。
  if (animation == RenderAnimation::Hit && (variant & 1) == 1) {
    candidates.insert(candidates.begin(), "Hit_B");
  } else if (animation == RenderAnimation::Death && (variant & 1) == 1) {
    candidates.insert(candidates.begin(), "Death_B");
  }
  // 跳跃/落地回退链：空中缺 Jump_Idle 时回退完整跳，落地缺
  // Jump_Land 时回退待机；资产无跳跃 clip 时行为与升级前一致。
  if (animation == RenderAnimation::Jump) {
    candidates.push_back("Jump_Full_Short");
  } else if (animation == RenderAnimation::Land) {
    candidates.push_back("idle");
  }
  if (animation == RenderAnimation::Dodge) {
    // 主角重制模型无方向闪避 clip：回退俯冲翻滚（Dive）作为闪避
    // 运动语言，再缺失时回退 run（与升级前行为一致）。
    candidates.push_back("Dive");
    candidates.push_back("run");
  } else if (animation == RenderAnimation::Radiance ||
             animation == RenderAnimation::Current ||
             animation == RenderAnimation::Corruption ||
             animation == RenderAnimation::Ultimate) {
    // 主角重制模型施法语言：三源施法与终结技统一回退 cast 吟唱
    // clip，再缺失时回退 attack（KayKit 资产行为不变）。
    candidates.push_back("cast");
    candidates.push_back("attack");
  } else if (animation == RenderAnimation::Climb) {
    // 攀爬回退链：无 climb clip 的资产沿用跑动语言（升级前行为）。
    candidates.push_back("run");
  } else if (animation == RenderAnimation::Glide) {
    // 滑翔回退链：无 glide clip 时回退 KayKit 空中姿态语言。
    candidates.push_back("Jump_Idle");
    candidates.push_back("Jump_Full_Short");
  }
  if (animation != RenderAnimation::Idle) candidates.push_back("idle");

  for (const std::string& candidate : candidates) {
    for (const std::string& clip : clips) {
      if (clip == candidate) return clip;
    }
  }
  return clips.empty() ? std::string{} : clips.front();
}
