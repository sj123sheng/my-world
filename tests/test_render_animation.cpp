#include "native/engine/render/render_animation.h"
#include "native/engine/render/combat_animation.h"
#include "native/engine/render/render_lifecycle.h"
#include "native/engine/render/skinned_model.h"
#include "native/engine/render/surface.h"
#include "native/gameplay/ai/encounter_controller.h"

#include <cassert>
#include <string>
#include <vector>
#include <cmath>

namespace {

void testAnimationPriority() {
  ActorRenderState actor;
  actor.alive = false;
  actor.action = RenderAnimation::Attack;
  actor.hit = true;
  actor.moving = true;
  assert(ChooseAnimation(actor) == RenderAnimation::Death);

  actor.alive = true;
  assert(ChooseAnimation(actor) == RenderAnimation::Attack);

  actor.action = RenderAnimation::Idle;
  assert(ChooseAnimation(actor) == RenderAnimation::Hit);

  actor.hit = false;
  assert(ChooseAnimation(actor) == RenderAnimation::Run);

  actor.moving = false;
  assert(ChooseAnimation(actor) == RenderAnimation::Idle);
}

void testClipResolutionFallsBackToIdle() {
  assert(ResolveClip({"idle"}, RenderAnimation::Attack) == "idle");
  assert(ResolveClip({"idle", "run", "attack"},
                     RenderAnimation::Attack) == "attack");
  assert(ResolveClip({"run"}, RenderAnimation::Death) == "run");
  assert(ResolveClip({}, RenderAnimation::Idle).empty());
  // 闪避专用剪辑存在时优先使用，缺失时回退 run。
  assert(ResolveClip({"idle", "run", "Dodge_Forward"},
                     RenderAnimation::Dodge) == "Dodge_Forward");
  assert(ResolveClip({"idle", "run"}, RenderAnimation::Dodge) == "run");
}

void testAttackFallsBackToCastForRemadePlayer() {
  // 重制主角 clip 集无 attack：普攻回退施法语言 cast（与技能
  // 释放同语言），cast 也缺失时由 idle 兜底。
  const std::vector<std::string> playerClips{
      "idle", "walk", "run", "Jump_Idle", "glide",
      "cast", "Dive", "Turn_180", "climb"};
  assert(ResolveClip(playerClips, RenderAnimation::Attack) == "cast");
  // 连段段数偏好 clip 缺失时同样回退 cast。
  assert(ResolveClip(playerClips, RenderAnimation::Attack, 0, 1.0f,
                     "1H_Melee_Attack_Slice_Diagonal") == "cast");
  // KayKit 资产有 attack：行为不变，attack 优先于 cast。
  assert(ResolveClip({"idle", "run", "attack", "cast"},
                     RenderAnimation::Attack) == "attack");
}

void testDedicatedActionClipNames() {
  assert(std::string(RenderAnimationName(RenderAnimation::Dodge)) ==
         "Dodge_Forward");
  assert(std::string(RenderAnimationName(RenderAnimation::Radiance)) ==
         "Spellcast_Raise");
  assert(std::string(RenderAnimationName(RenderAnimation::Current)) ==
         "Spellcast_Shoot");
  assert(std::string(RenderAnimationName(RenderAnimation::Corruption)) ==
         "Spellcasting");
  assert(std::string(RenderAnimationName(RenderAnimation::Ultimate)) ==
         "Spellcast_Long");
}

void testDedicatedActionClipFallbacks() {
  assert(ResolveClip({"idle", "run"}, RenderAnimation::Dodge) == "run");
  assert(ResolveClip({"idle", "attack"}, RenderAnimation::Radiance) ==
         "attack");
  assert(ResolveClip({"idle", "attack"}, RenderAnimation::Current) ==
         "attack");
  assert(ResolveClip({"idle", "attack"}, RenderAnimation::Corruption) ==
         "attack");
  assert(ResolveClip({"idle", "attack"}, RenderAnimation::Ultimate) ==
         "attack");
}

void testExplicitActionPriority() {
  ActorRenderState actor;
  actor.action = RenderAnimation::Dodge;
  actor.hit = true;
  actor.moving = true;
  assert(ChooseAnimation(actor) == RenderAnimation::Dodge);
  actor.alive = false;
  assert(ChooseAnimation(actor) == RenderAnimation::Death);
}

void testPlayerCombatActionMapsToDedicatedAnimation() {
  assert(PlayerRenderAnimation(ActionState::Idle, CombatAction::Attack) ==
         RenderAnimation::Idle);
  assert(PlayerRenderAnimation(ActionState::Attack1, CombatAction::Attack) ==
         RenderAnimation::Attack);
  assert(PlayerRenderAnimation(ActionState::Attack4, CombatAction::Attack) ==
         RenderAnimation::Attack);
  assert(PlayerRenderAnimation(ActionState::Dodging, CombatAction::Dodge) ==
         RenderAnimation::Dodge);
  assert(PlayerRenderAnimation(ActionState::CastingSource,
                               CombatAction::Radiance) ==
         RenderAnimation::Radiance);
  assert(PlayerRenderAnimation(ActionState::CastingSource,
                               CombatAction::Current) ==
         RenderAnimation::Current);
  assert(PlayerRenderAnimation(ActionState::CastingSource,
                               CombatAction::Corruption) ==
         RenderAnimation::Corruption);
  assert(PlayerRenderAnimation(ActionState::CastingUltimate,
                               CombatAction::Ultimate) ==
         RenderAnimation::Ultimate);
}

void testBossSnapshotMapsToCastHitDeathAndIdle() {
  BossSnapshot boss;
  boss.hp = fp(1000);
  assert(BossRenderAnimation(boss, fp(1000)) == RenderAnimation::Idle);

  boss.castRemainingMs = 500;
  assert(BossRenderAnimation(boss, fp(1000)) == RenderAnimation::Ultimate);

  boss.castRemainingMs = 0;
  boss.hp = fp(900);
  assert(BossRenderAnimation(boss, fp(1000)) == RenderAnimation::Hit);

  boss.castRemainingMs = 500;
  boss.defeated = true;
  assert(BossRenderAnimation(boss, fp(1000)) == RenderAnimation::Death);
}

void testAnimationLogOnlyReportsIntentOrResolvedClipChanges() {
  AnimationLogState logState;
  assert(logState.shouldReport(RenderAnimation::Idle, "Idle"));
  assert(!logState.shouldReport(RenderAnimation::Idle, "Idle"));
  assert(logState.shouldReport(RenderAnimation::Run, "Running_A"));
  assert(!logState.shouldReport(RenderAnimation::Run, "Running_A"));
  assert(logState.shouldReport(RenderAnimation::Run, "Running_B"));
}

void testUnavailableRuntimeModelStaysOnFallbackPath() {
  SkinnedModel model;
  assert(!model.ready());
  assert(!model.tryInitialize({0x67, 0x6c, 0x54, 0x46}, "player.glb"));
  assert(!model.ready());
  assert(model.lastError().find("runtime loader") != std::string::npos);
  model.destroy();
  assert(!model.ready());
}

void testSurfaceStoresLateModelAssetsForContextBoundInitialization() {
  Surface surface;
  surface.setModelAsset(ModelKind::Player, {1, 2, 3});
  surface.setModelAsset(ModelKind::Enemy, {4, 5});
  surface.setModelAsset(ModelKind::Boss, {6});

  assert((surface.playerModelAsset.bytes == std::vector<uint8_t>{1, 2, 3}));
  assert((surface.enemyModelAsset.bytes == std::vector<uint8_t>{4, 5}));
  assert((surface.bossModelAsset.bytes == std::vector<uint8_t>{6}));
  assert(surface.playerModelAsset.dirty);
  assert(surface.enemyModelAsset.dirty);
  assert(surface.bossModelAsset.dirty);
}

void testSurfaceKeepsEnvironmentBytesUntilContextBoundInitialization() {
  Surface surface;
  surface.setEnvironmentAsset(EnvironmentBatchKind::OuterRing, {1, 2, 3});
  assert(surface.environmentAssets[0].dirty);
  assert(surface.environmentAssets[0].bytes.size() == 3);
}

void testEnvironmentFailureKeepsFallbackEnabled() {
  Surface surface;
  surface.environmentStatuses[0] = EnvironmentBatchStatus::Failed;
  assert(surface.shouldDrawEnvironmentFallback());
}

void testSurfaceKeepsEnemyAnimationStateByStableEntityId() {
  Surface surface;
  surface.enemyAnimationStates.emplace(2001, SkinnedAnimationState{});
  surface.enemyAnimationStates.emplace(2002, SkinnedAnimationState{});

  Enemy3DRenderState remaining;
  remaining.id = 2002;
  surface.enemies3d.push_back(remaining);
  surface.pruneEnemyAnimationStates();

  assert(surface.enemyAnimationStates.size() == 1);
  assert(surface.enemyAnimationStates.find(2002) !=
         surface.enemyAnimationStates.end());
}

void testPendingAssetIsConsumedExactlyOnceAfterLateDirtySignal() {
  PendingModelAsset asset;
  std::vector<uint8_t> consumed;

  asset.replace({1, 2, 3});
  assert(asset.dirty);
  assert(asset.take(consumed));
  assert((consumed == std::vector<uint8_t>{1, 2, 3}));
  assert(!asset.dirty);
  assert(!asset.take(consumed));
}

void testPendingAssetReplacementAndClearRemainConsumable() {
  PendingModelAsset asset;
  std::vector<uint8_t> consumed;

  asset.replace({1});
  assert(asset.take(consumed));
  asset.replace({2, 3});
  assert(asset.take(consumed));
  assert((consumed == std::vector<uint8_t>{2, 3}));
  asset.replace({});
  assert(asset.take(consumed));
  assert(consumed.empty());
  assert(!asset.dirty);
}

void testSurfaceDestroyDoesNotTouchGlOrUnbindAfterMakeCurrentFailure() {
  std::vector<std::string> calls;
  SurfaceDestroyOperations operations;
  operations.makeCurrent = [&calls] {
    calls.emplace_back("make-current");
    return false;
  };
  operations.destroyGlResource = [&calls](SurfaceGlResource resource) {
    calls.emplace_back("gl-destroy-" + std::to_string(static_cast<int>(resource)));
  };
  operations.abandonGpuResources = [&calls] { calls.emplace_back("abandon-cpu"); };
  operations.unbindCurrent = [&calls] { calls.emplace_back("unbind"); };
  operations.destroyEglSurface = [&calls] { calls.emplace_back("destroy-egl-surface"); };
  operations.destroyEglContext = [&calls] { calls.emplace_back("destroy-egl-context"); };
  operations.terminateEglDisplay = [&calls] { calls.emplace_back("terminate-egl-display"); };

  ExecuteSurfaceDestroy(operations);

  assert((calls == std::vector<std::string>{
                       "make-current", "abandon-cpu", "destroy-egl-surface",
                       "destroy-egl-context", "terminate-egl-display"}));
}

void testSurfaceDestroyDestroysGlBeforeUnbindAndEglCleanup() {
  std::vector<std::string> calls;
  SurfaceDestroyOperations operations;
  operations.makeCurrent = [&calls] {
    calls.emplace_back("make-current");
    return true;
  };
  operations.destroyGlResource = [&calls](SurfaceGlResource resource) {
    switch (resource) {
      case SurfaceGlResource::SkinnedModels:
        calls.emplace_back("destroy-skinned-models");
        break;
      case SurfaceGlResource::StaticEnvironmentModels:
        calls.emplace_back("destroy-environment-models");
        break;
      case SurfaceGlResource::StaticMeshes:
        calls.emplace_back("destroy-static-meshes");
        break;
      case SurfaceGlResource::BloomPipeline:
        calls.emplace_back("destroy-bloom-pipeline");
        break;
      case SurfaceGlResource::Shader3D:
        calls.emplace_back("destroy-shader-3d");
        break;
      case SurfaceGlResource::Program2D:
        calls.emplace_back("destroy-program-2d");
        break;
    }
  };
  operations.abandonGpuResources = [&calls] { calls.emplace_back("abandon-cpu"); };
  operations.unbindCurrent = [&calls] { calls.emplace_back("unbind"); };
  operations.destroyEglSurface = [&calls] { calls.emplace_back("destroy-egl-surface"); };
  operations.destroyEglContext = [&calls] { calls.emplace_back("destroy-egl-context"); };
  operations.terminateEglDisplay = [&calls] { calls.emplace_back("terminate-egl-display"); };

  ExecuteSurfaceDestroy(operations);

  assert((calls == std::vector<std::string>{
                       "make-current", "destroy-skinned-models", "destroy-environment-models",
                       "destroy-static-meshes",
                       "destroy-bloom-pipeline",
                       "destroy-shader-3d", "destroy-program-2d", "unbind",
                       "destroy-egl-surface", "destroy-egl-context", "terminate-egl-display"}));
}

void testEnemy3DRenderStateHasAngleField() {
  Enemy3DRenderState enemy;
  assert(enemy.angle == 0.0f);
  enemy.angle = 1.5f;
  assert(enemy.angle == 1.5f);
}

void testBoss3DRenderStateHasAngleField() {
  Boss3DRenderState boss;
  assert(boss.angle == 0.0f);
  boss.angle = 2.3f;
  assert(boss.angle == 2.3f);
}

void testEncounterEnemySnapshotHasFacingField() {
  EncounterEnemySnapshot enemy;
  assert(enemy.facing.x == 1.0f);
  assert(enemy.facing.y == 0.0f);

  enemy.facing = {0.6f, 0.8f};
  const float angle = std::atan2(enemy.facing.x, enemy.facing.y);
  assert(std::abs(angle - 0.643501109f) < 0.001f);
}

void testEncounterEnemySnapshotEqualityIncludesFacing() {
  EncounterEnemySnapshot a;
  EncounterEnemySnapshot b;
  a.facing = {1.0f, 0.0f};
  b.facing = {0.0f, 1.0f};
  assert(!(a == b));
  b.facing = {1.0f, 0.0f};
  assert(a == b);
}

void testAnimationBlendDurationsMatchTransitionClasses() {
  const auto close = [](float actual, float expected) {
    return std::fabs(actual - expected) < 0.0001f;
  };
  // 移动状态互切保持原有快节奏混合。
  assert(close(AnimationBlendSeconds(RenderAnimation::Idle, RenderAnimation::Run), 0.15f));
  assert(close(AnimationBlendSeconds(RenderAnimation::Run, RenderAnimation::Idle), 0.15f));
  // 进入动作要快但有过渡，避免硬切。
  assert(close(AnimationBlendSeconds(RenderAnimation::Idle, RenderAnimation::Attack), 0.12f));
  assert(close(AnimationBlendSeconds(RenderAnimation::Run, RenderAnimation::Dodge), 0.12f));
  assert(close(AnimationBlendSeconds(RenderAnimation::Run, RenderAnimation::Hit), 0.12f));
  assert(close(AnimationBlendSeconds(RenderAnimation::Idle, RenderAnimation::Radiance), 0.12f));
  // 动作结束回到移动/待机用更长的恢复混合。
  assert(close(AnimationBlendSeconds(RenderAnimation::Attack, RenderAnimation::Idle), 0.2f));
  assert(close(AnimationBlendSeconds(RenderAnimation::Hit, RenderAnimation::Run), 0.2f));
  assert(close(AnimationBlendSeconds(RenderAnimation::Ultimate, RenderAnimation::Idle), 0.2f));
  // 死亡转场最长，保证倒地动作承接自然。
  assert(close(AnimationBlendSeconds(RenderAnimation::Run, RenderAnimation::Death), 0.25f));
  assert(close(AnimationBlendSeconds(RenderAnimation::Idle, RenderAnimation::Death), 0.25f));
  // 同一 Run 意图内，只有实际 clip 变化才补 0.15s 混合；主动动作
  // 缺 clip 回退时仍沿用原先按意图分类的时长。
  assert(close(AnimationBlendSeconds(RenderAnimation::Run, RenderAnimation::Run,
                                     "Walking_B", "run"),
               0.15f));
  assert(close(AnimationBlendSeconds(RenderAnimation::Run, RenderAnimation::Run,
                                     "run", "run"),
               0.0f));
  assert(close(AnimationBlendSeconds(RenderAnimation::Attack, RenderAnimation::Idle,
                                     "attack", "idle"),
               0.2f));
}

void testRunPlaybackRateTracksInputMagnitude() {
  const auto close = [](float actual, float expected) {
    return std::fabs(actual - expected) < 0.0001f;
  };
  // 满幅摇杆匹配动画原步频，半幅降速，下限保持步态稳定。
  assert(close(RunPlaybackRate(1.0f), 1.0f));
  assert(close(RunPlaybackRate(0.5f), 0.725f));
  assert(close(RunPlaybackRate(0.0f), 0.45f));
  // 越界输入被夹取。
  assert(close(RunPlaybackRate(2.0f), 1.0f));
  assert(close(RunPlaybackRate(-1.0f), 0.45f));
  // 单调：输入越大步频越快。
  assert(RunPlaybackRate(0.8f) > RunPlaybackRate(0.3f));
}

void testLoopingClipClassification() {
  // 待机/跑动与持续吟唱循环播放；攻击、受击、死亡、闪避、
  // 单次施法都是一次性 clip，必须钳制在尾帧而不是循环重播。
  assert(IsLoopingClip("idle"));
  assert(IsLoopingClip("run"));
  assert(IsLoopingClip("Spellcasting"));
  assert(!IsLoopingClip("attack"));
  assert(!IsLoopingClip("hit"));
  assert(!IsLoopingClip("death"));
  assert(!IsLoopingClip("Dodge_Forward"));
  assert(!IsLoopingClip("Spellcast_Long"));
}

void testDeathFadeAlphaHoldsThenFadesToZero() {
  const auto close = [](float actual, float expected) {
    return std::fabs(actual - expected) < 0.0001f;
  };
  // 死亡后先保持尾帧 0.35s，再用 0.55s 线性淡出到完全移除。
  assert(close(DeathFadeAlpha(0.0f), 1.0f));
  assert(close(DeathFadeAlpha(0.35f), 1.0f));
  assert(close(DeathFadeAlpha(0.625f), 0.5f));
  assert(close(DeathFadeAlpha(0.9f), 0.0f));
  assert(close(DeathFadeAlpha(5.0f), 0.0f));
  assert(close(DeathFadeAlpha(-1.0f), 1.0f));
  assert(DeathFadeAlpha(0.4f) > DeathFadeAlpha(0.8f));
}

void testClipVariantsAlternateByVariantIndex() {
  // 受击：变体 0 用主 hit，变体 1 优先 Hit_B，缺失时回退主 hit。
  const std::vector<std::string> withVariants{"idle", "run", "attack", "hit",
                                              "Hit_B", "death", "Death_B"};
  assert(ResolveClip(withVariants, RenderAnimation::Hit, 0) == "hit");
  assert(ResolveClip(withVariants, RenderAnimation::Hit, 1) == "Hit_B");
  assert(ResolveClip(withVariants, RenderAnimation::Death, 1) == "Death_B");
  // 变体 clip 缺失时回退主 clip，再走原有回退链。
  const std::vector<std::string> noVariants{"idle", "run", "attack", "hit",
                                            "death"};
  assert(ResolveClip(noVariants, RenderAnimation::Hit, 1) == "hit");
  assert(ResolveClip(noVariants, RenderAnimation::Death, 1) == "death");
  // 非受击/死亡动作不受变体影响。
  assert(ResolveClip(withVariants, RenderAnimation::Attack, 1) == "attack");
  assert(ResolveClip(withVariants, RenderAnimation::Idle, 1) == "idle");
}

void testLowSpeedLocomotionPrefersWalkClip() {
  // 低幅度输入切换行走 clip：阈值 0.35，低于时优先 Walking_B，
  // 缺失时回退 run；高速与非移动意图不受影响。
  assert(ShouldUseWalkClip(0.0f));
  assert(ShouldUseWalkClip(0.2f));
  assert(!ShouldUseWalkClip(0.35f));
  assert(!ShouldUseWalkClip(1.0f));
  const std::vector<std::string> clips{"idle", "run", "Walking_B"};
  assert(ResolveClip(clips, RenderAnimation::Run, 0, 0.35f, {},
                     LocomotionGait::Walk) == "Walking_B");
  assert(ResolveClip(clips, RenderAnimation::Run, 0, 0.35f, {},
                     LocomotionGait::Run) == "run");
  // 主角语义 clip 与兼容旧资产的 Walking_B 同时存在时，前者必须优先。
  assert(ResolveClip({"idle", "run", "walk", "Walking_B"},
                     RenderAnimation::Run, 0, 0.2f, {},
                     LocomotionGait::Walk) == "walk");
  const std::vector<std::string> noWalk{"idle", "run"};
  assert(ResolveClip(noWalk, RenderAnimation::Run, 0, 0.2f) == "run");
  assert(ResolveClip(clips, RenderAnimation::Idle, 0, 0.2f) == "idle");
}

void testLocomotionGaitUsesHysteresis() {
  assert(IsLoopingClip("Walking_B"));
  assert(ChooseLocomotionGait(LocomotionGait::Unknown, 0.20f) ==
         LocomotionGait::Walk);
  assert(ChooseLocomotionGait(LocomotionGait::Unknown, 0.35f) ==
         LocomotionGait::Run);
  assert(ChooseLocomotionGait(LocomotionGait::Walk, 0.39f) ==
         LocomotionGait::Walk);
  assert(ChooseLocomotionGait(LocomotionGait::Walk, 0.40f) ==
         LocomotionGait::Walk);
  assert(ChooseLocomotionGait(LocomotionGait::Walk, 0.41f) ==
         LocomotionGait::Run);
  assert(ChooseLocomotionGait(LocomotionGait::Run, 0.31f) ==
         LocomotionGait::Run);
  assert(ChooseLocomotionGait(LocomotionGait::Run, 0.30f) ==
         LocomotionGait::Run);
  assert(ChooseLocomotionGait(LocomotionGait::Run, 0.29f) ==
         LocomotionGait::Walk);

  const std::vector<std::string> clips{"idle", "run", "Walking_B"};
  assert(ResolveClip(clips, RenderAnimation::Run, 0, 0.35f, {},
                     LocomotionGait::Walk) == "Walking_B");
  assert(ResolveClip(clips, RenderAnimation::Run, 0, 0.35f, {},
                     LocomotionGait::Run) == "run");
}

void testAttackClipDifferentiationBySegmentArchetypeVariant() {
  // 主角连段：四段各不同，终结段为双手重劈，未知段数回退 attack。
  assert(PlayerAttackClipFor(1) ==
         std::string("1H_Melee_Attack_Slice_Diagonal"));
  assert(PlayerAttackClipFor(2) ==
         std::string("1H_Melee_Attack_Slice_Horizontal"));
  assert(PlayerAttackClipFor(3) == std::string("1H_Melee_Attack_Stab"));
  assert(PlayerAttackClipFor(4) == std::string("2H_Melee_Attack_Chop"));
  assert(PlayerAttackClipFor(0) == std::string("attack"));
  assert(PlayerAttackClipFor(9) == std::string("attack"));
  // 敌人原型：六类各不同，未知原型回退 attack。
  assert(EnemyAttackClipFor(0) ==
         std::string("Unarmed_Melee_Attack_Punch_A"));
  assert(EnemyAttackClipFor(1) == std::string("Spellcast_Raise"));
  assert(EnemyAttackClipFor(2) == std::string("Block_Attack"));
  assert(EnemyAttackClipFor(3) == std::string("2H_Melee_Attack_Chop"));
  assert(EnemyAttackClipFor(4) == std::string("Spellcast_Shoot"));
  assert(EnemyAttackClipFor(5) == std::string("2H_Melee_Attack_Spin"));
  assert(EnemyAttackClipFor(99) == std::string("attack"));
  // 首领变体：三变体循环，各不同。
  assert(BossAttackClipFor(0) == std::string("2H_Melee_Attack_Chop"));
  assert(BossAttackClipFor(1) == std::string("Spellcast_Long"));
  assert(BossAttackClipFor(2) == std::string("2H_Melee_Attack_Spin"));
  assert(BossAttackClipFor(3) == std::string("2H_Melee_Attack_Chop"));
  // ResolveClip：偏好 clip 存在时优先，缺失回退 attack；
  // 非攻击动作忽略偏好 clip。
  const std::vector<std::string> full{"idle", "run", "attack",
                                      "1H_Melee_Attack_Stab"};
  assert(ResolveClip(full, RenderAnimation::Attack, 0, 1.0f,
                     "1H_Melee_Attack_Stab") == "1H_Melee_Attack_Stab");
  const std::vector<std::string> bare{"idle", "run", "attack"};
  assert(ResolveClip(bare, RenderAnimation::Attack, 0, 1.0f,
                     "1H_Melee_Attack_Stab") == "attack");
  assert(ResolveClip(full, RenderAnimation::Idle, 0, 1.0f,
                     "1H_Melee_Attack_Stab") == "idle");
  // 连段段数映射：Attack1..Attack4 → 1..4，其余 0。
  assert(PlayerComboSegmentFor(ActionState::Attack1) == 1);
  assert(PlayerComboSegmentFor(ActionState::Attack2) == 2);
  assert(PlayerComboSegmentFor(ActionState::Attack3) == 3);
  assert(PlayerComboSegmentFor(ActionState::Attack4) == 4);
  assert(PlayerComboSegmentFor(ActionState::Idle) == 0);
  assert(PlayerComboSegmentFor(ActionState::Dodging) == 0);
}

void testEnemyWeaponKindMatchesAttackLanguage() {
  assert(EnemyWeaponKindFor(0) == 0);  // RiftClaw 徒手爪击
  assert(EnemyWeaponKindFor(1) == 1);  // Priest 法杖
  assert(EnemyWeaponKindFor(2) == 1);  // Guard 法杖
  assert(EnemyWeaponKindFor(3) == 3);  // Bruiser 重棍
  assert(EnemyWeaponKindFor(4) == 1);  // Caster 法杖
  assert(EnemyWeaponKindFor(5) == 2);  // Elite 长剑
  assert(EnemyWeaponKindFor(99) == 1); // 未知原型回退法杖
}

void testJumpAndLandAnimationClips() {
  // 动画意图名：空中 Jump_Idle、落地 Jump_Land。
  assert(std::string(RenderAnimationName(RenderAnimation::Jump)) ==
         "Jump_Idle");
  assert(std::string(RenderAnimationName(RenderAnimation::Land)) ==
         "Jump_Land");
  // 起跳 clip 选取：前 0.18s Jump_Start，之后空中 Jump_Idle。
  assert(std::string(PlayerJumpClipFor(0.0f)) == "Jump_Start");
  assert(std::string(PlayerJumpClipFor(0.17f)) == "Jump_Start");
  assert(std::string(PlayerJumpClipFor(0.18f)) == "Jump_Idle");
  assert(std::string(PlayerJumpClipFor(5.0f)) == "Jump_Idle");
  // ResolveClip：偏好起跳 clip 存在时优先；缺失按空中/完整跳/
  // 待机顺序回退。
  const std::vector<std::string> full{"idle",        "run",
                                      "Jump_Start",  "Jump_Idle",
                                      "Jump_Land",   "Jump_Full_Short"};
  assert(ResolveClip(full, RenderAnimation::Jump, 0, 1.0f, "Jump_Start") ==
         "Jump_Start");
  assert(ResolveClip(full, RenderAnimation::Jump, 0, 1.0f) == "Jump_Idle");
  assert(ResolveClip(full, RenderAnimation::Land, 0, 1.0f) == "Jump_Land");
  const std::vector<std::string> partial{"idle", "run", "Jump_Full_Short"};
  assert(ResolveClip(partial, RenderAnimation::Jump, 0, 1.0f) ==
         "Jump_Full_Short");
  assert(ResolveClip(partial, RenderAnimation::Land, 0, 1.0f) == "idle");
}


void testDirectionalDodgeClipFollowsRelativeAngle() {
  // 前/侧/后扇区划分（正角 = 左）。
  assert(std::string(PlayerDodgeClipFor(0.0f)) == "Dodge_Forward");
  assert(std::string(PlayerDodgeClipFor(0.5f)) == "Dodge_Forward");
  assert(std::string(PlayerDodgeClipFor(1.2f)) == "Dodge_Left");
  assert(std::string(PlayerDodgeClipFor(-1.2f)) == "Dodge_Right");
  assert(std::string(PlayerDodgeClipFor(3.0f)) == "Dodge_Backward");
  assert(std::string(PlayerDodgeClipFor(-3.0f)) == "Dodge_Backward");
  // 扇区边界：pi/4 与 3pi/4。
  assert(std::string(PlayerDodgeClipFor(0.7853981f)) == "Dodge_Forward");
  assert(std::string(PlayerDodgeClipFor(0.79f)) == "Dodge_Left");
  assert(std::string(PlayerDodgeClipFor(2.36f)) == "Dodge_Backward");
  // ResolveClip：闪避偏好 clip 存在时优先，缺失回退 Dodge_Forward→run。
  const std::vector<std::string> full{"idle", "run", "Dodge_Forward",
                                      "Dodge_Left", "Dodge_Right",
                                      "Dodge_Backward"};
  assert(ResolveClip(full, RenderAnimation::Dodge, 0, 1.0f, "Dodge_Left") ==
         "Dodge_Left");
  assert(ResolveClip(full, RenderAnimation::Dodge, 0, 1.0f,
                     "Dodge_Backward") == "Dodge_Backward");
  const std::vector<std::string> forwardOnly{"idle", "run", "Dodge_Forward"};
  assert(ResolveClip(forwardOnly, RenderAnimation::Dodge, 0, 1.0f,
                     "Dodge_Left") == "Dodge_Forward");
  const std::vector<std::string> noDodge{"idle", "run"};
  assert(ResolveClip(noDodge, RenderAnimation::Dodge, 0, 1.0f) == "run");
}

void testRemadePlayerExplorationAnimationLanguage() {
  // 主角重制模型探索语言：攀爬/滑翔/转身的意图名与 clip 名。
  assert(std::string(RenderAnimationName(RenderAnimation::Climb)) == "climb");
  assert(std::string(RenderAnimationName(RenderAnimation::Glide)) == "glide");
  assert(std::string(RenderAnimationName(RenderAnimation::Turn)) ==
         "Turn_180");

  // 重制主角 clip 集（prepare_player_glb.py 产物）：新语言全部命中，
  // 无 attack 时普攻回退施法语言 cast（与技能释放同语言），
  // 无 hit/death 时回退 idle（刀光/特效承接反馈）。
  const std::vector<std::string> remade{"idle",  "walk",  "run",
                                        "Jump_Idle", "glide", "cast",
                                        "Dive",  "Turn_180", "climb"};
  assert(ResolveClip(remade, RenderAnimation::Climb) == "climb");
  assert(ResolveClip(remade, RenderAnimation::Glide) == "glide");
  assert(ResolveClip(remade, RenderAnimation::Turn) == "Turn_180");
  assert(ResolveClip(remade, RenderAnimation::Run, 0, 1.0f) == "run");
  // 低速步态：KayKit Walking_B 缺失时回退重制模型的 walk。
  assert(ResolveClip(remade, RenderAnimation::Run, 0, 0.2f) == "walk");
  // 空中：Jump_Start 缺失时回退 Jump_Idle（原地跳跃姿态循环）。
  assert(ResolveClip(remade, RenderAnimation::Jump, 0, 1.0f, "Jump_Start") ==
         "Jump_Idle");
  assert(ResolveClip(remade, RenderAnimation::Land) == "idle");
  // 闪避：方向闪避 clip 缺失时回退俯冲翻滚 Dive。
  assert(ResolveClip(remade, RenderAnimation::Dodge, 0, 1.0f,
                     "Dodge_Left") == "Dive");
  assert(ResolveClip(remade, RenderAnimation::Dodge) == "Dive");
  // 施法：三源/终结技统一回退 cast 吟唱。
  assert(ResolveClip(remade, RenderAnimation::Radiance) == "cast");
  assert(ResolveClip(remade, RenderAnimation::Current) == "cast");
  assert(ResolveClip(remade, RenderAnimation::Corruption) == "cast");
  assert(ResolveClip(remade, RenderAnimation::Ultimate) == "cast");
  // 普攻：重制模型无 attack clip，回退 cast 施法语言。
  assert(ResolveClip(remade, RenderAnimation::Attack) == "cast");
  // cast 也缺失时才继续回退 idle。
  assert(ResolveClip({"idle", "run"}, RenderAnimation::Attack) == "idle");
  assert(ResolveClip(remade, RenderAnimation::Hit) == "idle");
  assert(ResolveClip(remade, RenderAnimation::Death) == "idle");

  // KayKit 资产（enemy/boss）无新 clip：按回退链保持升级前行为。
  const std::vector<std::string> kaykit{"idle",   "run",    "attack",
                                        "hit",    "death",  "Jump_Idle",
                                        "Walking_B"};
  assert(ResolveClip(kaykit, RenderAnimation::Climb) == "run");
  assert(ResolveClip(kaykit, RenderAnimation::Glide) == "Jump_Idle");
  assert(ResolveClip(kaykit, RenderAnimation::Turn) == "idle");
  assert(ResolveClip(kaykit, RenderAnimation::Run, 0, 0.2f) == "Walking_B");

  // 循环分类：walk/glide/cast/Jump_Idle 循环播放；Dive/Turn_180/climb
  // 一次性钳制尾帧。
  assert(IsLoopingClip("walk"));
  assert(IsLoopingClip("glide"));
  assert(IsLoopingClip("cast"));
  assert(IsLoopingClip("Jump_Idle"));
  assert(!IsLoopingClip("Dive"));
  assert(!IsLoopingClip("Turn_180"));
  assert(!IsLoopingClip("climb"));
}

void testWeaponJointFallbackChain() {
  // handslot.r 优先；自定义骨架回退 R_Hand/RightHand/mixamorig；均无 -1。
  assert(FindWeaponJointIndex({"Root", "handslot.r", "R_Hand"}) == 1);
  assert(FindWeaponJointIndex({"Root", "R_Hand"}) == 1);
  assert(FindWeaponJointIndex({"Root", "RightHand"}) == 1);
  assert(FindWeaponJointIndex({"Root", "mixamorig:RightHand"}) == 1);
  assert(FindWeaponJointIndex({"Root", "Hips"}) == -1);
  assert(FindWeaponJointIndex({}) == -1);
}

}  // namespace


int main() {
  testAnimationPriority();
  testClipResolutionFallsBackToIdle();
  testAttackFallsBackToCastForRemadePlayer();
  testDedicatedActionClipNames();
  testDedicatedActionClipFallbacks();
  testExplicitActionPriority();
  testPlayerCombatActionMapsToDedicatedAnimation();
  testBossSnapshotMapsToCastHitDeathAndIdle();
  testAnimationBlendDurationsMatchTransitionClasses();
  testRunPlaybackRateTracksInputMagnitude();
  testLoopingClipClassification();
  testDeathFadeAlphaHoldsThenFadesToZero();
  testClipVariantsAlternateByVariantIndex();
  testLowSpeedLocomotionPrefersWalkClip();
  testLocomotionGaitUsesHysteresis();
  testAttackClipDifferentiationBySegmentArchetypeVariant();
  testEnemyWeaponKindMatchesAttackLanguage();
  testJumpAndLandAnimationClips();
  testDirectionalDodgeClipFollowsRelativeAngle();
  testRemadePlayerExplorationAnimationLanguage();
  testWeaponJointFallbackChain();
  testAnimationLogOnlyReportsIntentOrResolvedClipChanges();
  testUnavailableRuntimeModelStaysOnFallbackPath();
  testSurfaceStoresLateModelAssetsForContextBoundInitialization();
  testSurfaceKeepsEnvironmentBytesUntilContextBoundInitialization();
  testEnvironmentFailureKeepsFallbackEnabled();
  testSurfaceKeepsEnemyAnimationStateByStableEntityId();
  testPendingAssetIsConsumedExactlyOnceAfterLateDirtySignal();
  testPendingAssetReplacementAndClearRemainConsumable();
  testSurfaceDestroyDoesNotTouchGlOrUnbindAfterMakeCurrentFailure();
  testSurfaceDestroyDestroysGlBeforeUnbindAndEglCleanup();
  testEnemy3DRenderStateHasAngleField();
  testBoss3DRenderStateHasAngleField();
  testEncounterEnemySnapshotHasFacingField();
  testEncounterEnemySnapshotEqualityIncludesFacing();
  return 0;
}
